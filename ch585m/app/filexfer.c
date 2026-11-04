/********************************** (C) COPYRIGHT *******************************
 * File Name          : filexfer.c
 * Description        : Synthetic BMP file generator, streamed over EP2
 *                       bulk IN for CH585 USBHS throughput testing.
 *
 *  No full frame is stored. Lookup tables and optional packed blue lanes
 *  are precomputed at init.
 *  A producer prepares aligned RGB rows while USB DMA transmits ready data.
 *
 *  FileXfer_Pump() generates rows in main context and publishes each only
 *  after all stores complete. The ISR supplies pointers to ready contiguous
 *  data, or NAKs until ready. Rows remain USB-owned until DMA completion.
 *
 *  BMP layout (24-bit uncompressed, BITMAPINFOHEADER, 54-byte header):
 *  rows are stored bottom-to-top, each row BGR8:8:8, row size already a
 *  multiple of 4 bytes (no padding needed for width=1024).
 *******************************************************************************/
#include "filexfer.h"
#include "ch585_usbhs_device.h"
#include <string.h>

#ifndef FILEXFER_STATIC_PATTERN
#define FILEXFER_STATIC_PATTERN 0
#endif
#if FILEXFER_STATIC_PATTERN
#define STATIC_PATTERN_BYTES 32768u
static uint8_t s_staticPattern[STATIC_PATTERN_BYTES] __attribute__((aligned(4)));
#endif


/* Set by the USB ISR when it had to NAK because the ring ran dry; the
 * main-loop pump re-arms EP2 after refilling. Generation must NEVER happen
 * in interrupt context: at transfer rates an in-ISR generator burns more
 * CPU than the main loop has left, starving the pump so the ring stays
 * empty forever - the device deadlocks itself at ~1/4 of the achievable
 * throughput (measured 244 KB/s before this was enforced). */
static volatile uint8_t s_needArm = 0;

/* Set by the USB ISR (START_FILE_XFER, EP0 SETUP context) to request a
 * restart; the main-loop pump performs the state reset + prefill + arm,
 * keeping row generation in main context. */
static volatile uint8_t s_startReq = 0;


/* True until the final DMA completion has been accounted for. */
static volatile uint8_t s_active = 0;

/* Completed (not merely queued) bytes, published by the USB completion path. */
static volatile uint32_t s_genPos;

/* Colorful gradient with zero-copy serving: rotating row buffers;
 * the pump generates rows a few ahead of the send position (cheap LUT
 * walk per row) while the endpoint DMA reads finished rows directly. */
#define ROW_BYTES   (FILEXFER_IMG_WIDTH * 3u)
#ifndef FILEXFER_ROW_BUF_CNT
#define FILEXFER_ROW_BUF_CNT 4u
#endif
#define ROW_BUF_CNT FILEXFER_ROW_BUF_CNT
#if ROW_BUF_CNT < 2 || ROW_BUF_CNT > 32
#error "FILEXFER_ROW_BUF_CNT must be between 2 and 32"
#endif
typedef union
{
    uint8_t bytes[ROW_BYTES];
    uint32_t words[ROW_BYTES / 4u];
} FileRow;
#if !FILEXFER_STATIC_PATTERN
static FileRow s_rows[ROW_BUF_CNT] __attribute__((aligned(4)));
static uint8_t s_blueByCol[FILEXFER_IMG_WIDTH];
static uint8_t s_greenByRow[FILEXFER_IMG_HEIGHT];
#endif
static volatile uint32_t s_genRow = 0;   /* fully generated, published to ISR */
/* Returned DMA data remains owned by USB until the next completion callback. */
static uint16_t s_pendingLen;

#ifndef FILEXFER_RAM_GENERATOR
#define FILEXFER_RAM_GENERATOR 0
#endif
#if FILEXFER_RAM_GENERATOR
#define FILEXFER_HOT __HIGH_CODE
#else
#define FILEXFER_HOT
#endif

#ifndef FILEXFER_PACKED_GENERATOR
#define FILEXFER_PACKED_GENERATOR 0
#endif
#if FILEXFER_PACKED_GENERATOR && !FILEXFER_STATIC_PATTERN
#if (FILEXFER_IMG_WIDTH % 4) != 0
#error "Packed RGB generation needs a width divisible by four"
#endif
/* Four RGB pixels occupy exactly three aligned words. Their blue lanes never
 * change across rows; green lanes are broadcast and red lanes shift per row. */
static uint32_t s_blueWords[ROW_BYTES / 4u];
#endif

#if !FILEXFER_STATIC_PATTERN
static FILEXFER_HOT void GenerateRow(uint32_t rowFromTop)
{
    uint8_t green = s_greenByRow[FILEXFER_IMG_HEIGHT - 1u - rowFromTop];
#if FILEXFER_PACKED_GENERATOR
    uint32_t *dst = s_rows[rowFromTop % ROW_BUF_CNT].words;
    const uint32_t *blue = s_blueWords;
    uint32_t g0 = (uint32_t)green << 8;
    uint32_t g1 = (uint32_t)green | ((uint32_t)green << 24);
    uint32_t g2 = (uint32_t)green << 16;
    uint8_t red = (uint8_t)(FILEXFER_IMG_HEIGHT - 1u - rowFromTop);
    for (uint32_t col = 0; col < FILEXFER_IMG_WIDTH; col += 4)
    {
        dst[0] = blue[0] | g0 | ((uint32_t)red << 16);
        dst[1] = blue[1] | g1 | ((uint32_t)(uint8_t)(red + 1) << 8);
        dst[2] = blue[2] | g2 | (uint32_t)(uint8_t)(red + 2) |
                 ((uint32_t)(uint8_t)(red + 3) << 24);
        dst += 3;
        blue += 3;
        red = (uint8_t)(red + 4);
    }
#else
    uint8_t *dst = s_rows[rowFromTop % ROW_BUF_CNT].bytes;
    uint32_t col;
    for (col = 0; col < FILEXFER_IMG_WIDTH; col++)
    {
        *dst++ = s_blueByCol[col];
        *dst++ = green;
        *dst++ = (uint8_t)(col + (FILEXFER_IMG_HEIGHT - 1u - rowFromTop));
    }
#endif
}
#endif
static uint8_t s_header[FILEXFER_BMP_HEADER_LEN] __attribute__((aligned(4)));

/* Builds the 54-byte BMP header into 'hdr'. */
static void BuildBmpHeader(uint8_t *hdr)
{
    uint32_t fileSize  = FILEXFER_TOTAL_SIZE;
    uint32_t dataOffset = FILEXFER_BMP_HEADER_LEN;
    uint32_t dibSize    = 40;
    uint32_t width      = FILEXFER_IMG_WIDTH;
    uint32_t height     = FILEXFER_IMG_HEIGHT;
    uint32_t imgSize    = FILEXFER_IMG_WIDTH * FILEXFER_IMG_HEIGHT * 3u;

    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2]  = (uint8_t)(fileSize);       hdr[3]  = (uint8_t)(fileSize >> 8);
    hdr[4]  = (uint8_t)(fileSize >> 16); hdr[5]  = (uint8_t)(fileSize >> 24);
    hdr[6] = 0; hdr[7] = 0; hdr[8] = 0; hdr[9] = 0;                          /* reserved */
    hdr[10] = (uint8_t)(dataOffset);       hdr[11] = (uint8_t)(dataOffset >> 8);
    hdr[12] = (uint8_t)(dataOffset >> 16); hdr[13] = (uint8_t)(dataOffset >> 24);
    hdr[14] = (uint8_t)(dibSize);       hdr[15] = (uint8_t)(dibSize >> 8);
    hdr[16] = (uint8_t)(dibSize >> 16); hdr[17] = (uint8_t)(dibSize >> 24);
    hdr[18] = (uint8_t)(width);       hdr[19] = (uint8_t)(width >> 8);
    hdr[20] = (uint8_t)(width >> 16); hdr[21] = (uint8_t)(width >> 24);
    hdr[22] = (uint8_t)(height);       hdr[23] = (uint8_t)(height >> 8);
    hdr[24] = (uint8_t)(height >> 16); hdr[25] = (uint8_t)(height >> 24);
    hdr[26] = 1; hdr[27] = 0;   /* planes = 1 */
    hdr[28] = 24; hdr[29] = 0;  /* bits per pixel = 24 */
    hdr[30] = 0; hdr[31] = 0; hdr[32] = 0; hdr[33] = 0; /* compression = 0 (BI_RGB) */
    hdr[34] = (uint8_t)(imgSize);       hdr[35] = (uint8_t)(imgSize >> 8);
    hdr[36] = (uint8_t)(imgSize >> 16); hdr[37] = (uint8_t)(imgSize >> 24);
    hdr[38] = 0; hdr[39] = 0; hdr[40] = 0; hdr[41] = 0; /* x pixels/meter */
    hdr[42] = 0; hdr[43] = 0; hdr[44] = 0; hdr[45] = 0; /* y pixels/meter */
    hdr[46] = 0; hdr[47] = 0; hdr[48] = 0; hdr[49] = 0; /* colors used */
    hdr[50] = 0; hdr[51] = 0; hdr[52] = 0; hdr[53] = 0; /* important colors */
}

/* Called from the idle main loop: fills the ring buffer with as many
 * freshly-generated packets as there is room for, then re-arms EP2 if the
 * ISR previously had to NAK on an empty ring. Cheap to call often - it's
 * a no-op once the ring is full or the file is exhausted. */
void FileXfer_Pump(void)
{
    if (s_startReq)
    {
        s_startReq = 0;
        s_genPos = 0;
        s_pendingLen = 0;
        s_genRow = 0;
        s_active = 1;
#if !FILEXFER_STATIC_PATTERN
        while ((s_genRow < ROW_BUF_CNT - 1u) && (s_genRow < FILEXFER_IMG_HEIGHT))
        {
            GenerateRow(s_genRow);
            __asm volatile("" ::: "memory");
            s_genRow++;
        }
#endif
        s_needArm = 1;
    }

    /* keep generating rows ahead of the send position */
#if !FILEXFER_STATIC_PATTERN
    if (s_active && (s_genPos > FILEXFER_BMP_HEADER_LEN))
    {
        uint32_t sendRow = (s_genPos - FILEXFER_BMP_HEADER_LEN) / ROW_BYTES;
        while ((s_genRow < sendRow + ROW_BUF_CNT - 1u) && (s_genRow < FILEXFER_IMG_HEIGHT))
        {
            GenerateRow(s_genRow);
            __asm volatile("" ::: "memory");
            s_genRow++;
        }
    }
#endif

    if (s_active && (s_genPos >= FILEXFER_TOTAL_SIZE))
    {
        s_active = 0;
    }

    if (s_needArm)
    {
        s_needArm = 0;
        USBD_EP2_StartTransfer();
    }
}

/* USB-interrupt-context callback: returns a zero-copy ready span.
 * If the ring is dry it returns 0, which NAKs
 * the endpoint - the host retries while the main-loop pump refills and
 * re-arms from FileXfer_Pump(). No generation happens here (see
 * s_needArm). */
static uint16_t FileXfer_FillCallback(const uint8_t **pptr, uint16_t maxlen)
{
    /* The driver invokes us only while idle / after IN completion. Advancing
     * on completion, not submission, keeps the producer off in-flight rows. */
    s_genPos += s_pendingLen;
    s_pendingLen = 0;
    uint32_t pos = s_genPos;
    uint32_t remain = FILEXFER_TOTAL_SIZE - pos;

    *pptr = NULL;
    if (remain == 0)
    {
        return 0; /* end of file: stay NAKed */
    }

    if (pos < FILEXFER_BMP_HEADER_LEN)
    {
        /* the 54-byte header is its own (short) first packet, so every
         * later packet is 512-aligned inside a 3072-byte row */
        uint16_t n = (maxlen < FILEXFER_BMP_HEADER_LEN) ? maxlen : (uint16_t)FILEXFER_BMP_HEADER_LEN;
        *pptr = s_header;
        s_pendingLen = n;
        return n;
    }

    {
        uint32_t pix = pos - FILEXFER_BMP_HEADER_LEN;
#if FILEXFER_STATIC_PATTERN
        uint32_t off = pix % STATIC_PATTERN_BYTES;
        uint32_t avail = STATIC_PATTERN_BYTES - off;
        if (avail > remain) avail = remain;
        if (avail > maxlen) avail = maxlen;
        *pptr = &s_staticPattern[off];
        s_pendingLen = (uint16_t)avail;
        return (uint16_t)avail;
#else
        uint32_t rowLen = ROW_BYTES;
        uint32_t row = pix / rowLen;
        uint32_t off = pix % rowLen;
        const uint8_t *src = s_rows[row % ROW_BUF_CNT].bytes;
        if (row >= s_genRow)
        {
            /* A faster host may drain the prepared rows before the main loop
             * runs. NAK rather than DMA a stale/partially generated row. */
            s_needArm = 1;
            return 0;
        }
        uint32_t avail = rowLen - off;
#if USBHS_EP2_BURST_BYTES
        /* Adjacent ready rows are physically contiguous until the ring wraps.
         * s_genPos is a COMPLETED cursor, so the producer cannot reuse any
         * row held by this multi-packet DMA burst. */
        uint32_t readyRows = s_genRow - row;
        uint32_t physicalRows = ROW_BUF_CNT - (row % ROW_BUF_CNT);
        if (readyRows > physicalRows)
        {
            readyRows = physicalRows;
        }
        avail = readyRows * rowLen - off;
#endif
        if (avail > remain)
        {
            avail = remain;
        }
        if (avail > maxlen)
        {
            avail = maxlen;
        }
        *pptr = &src[off];
        s_pendingLen = (uint16_t)avail;
        return (uint16_t)avail;
#endif
    }
}

void FileXfer_Init(void)
{
    uint32_t i;

    BuildBmpHeader(s_header);
#if FILEXFER_STATIC_PATTERN
    for (i = 0; i < STATIC_PATTERN_BYTES; i++)
    {
        s_staticPattern[i] = (uint8_t)i ^ (uint8_t)((i / 512u) * 61u);
    }
#endif

#if !FILEXFER_STATIC_PATTERN
    for (i = 0; i < FILEXFER_IMG_WIDTH; i++)
    {
        s_blueByCol[i] = (uint8_t)((i * 255u) / (FILEXFER_IMG_WIDTH - 1u));
    }
    for (i = 0; i < FILEXFER_IMG_HEIGHT; i++)
    {
        s_greenByRow[i] = (uint8_t)((i * 255u) / (FILEXFER_IMG_HEIGHT - 1u));
    }
#if FILEXFER_PACKED_GENERATOR
    for (i = 0; i < FILEXFER_IMG_WIDTH; i += 4)
    {
        uint32_t *words = &s_blueWords[(i / 4) * 3];
        words[0] = (uint32_t)s_blueByCol[i] | ((uint32_t)s_blueByCol[i + 1] << 24);
        words[1] = (uint32_t)s_blueByCol[i + 2] << 16;
        words[2] = (uint32_t)s_blueByCol[i + 3] << 8;
    }
#endif
#endif

    USBD_EP2_SetFillCallback(FileXfer_FillCallback);
}

void FileXfer_Start(void)
{
    /* Called from EP0 SETUP (USB ISR) context: only record the request;
     * FileXfer_Pump() in the main loop does the reset + prefill + arm. */
    USBD_EP2_StopTransfer();
    s_startReq = 1;
}
