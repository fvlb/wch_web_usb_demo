#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filexfer.h"
#include "ch585_usbhs_device.h"

static USBD_EP2_FillCallback fill;
static const uint8_t *dma;
static uint16_t length;
static unsigned busy;
static unsigned starvation;

void USBD_EP2_SetFillCallback(USBD_EP2_FillCallback cb) { fill = cb; }
void USBD_EP2_StopTransfer(void) { busy = 0; }
void USBD_EP2_StartTransfer(void)
{
    if (busy) return;
    const uint8_t *p = NULL;
#if USBHS_EP2_BURST_BYTES
    unsigned limit = USBHS_EP2_BURST_BYTES;
#else
    unsigned limit = 512;
#endif
    length = fill(&p, (uint16_t)limit);
    if (!length) { starvation++; return; }
    assert(p && !((uintptr_t)p & 3) && length <= limit);
    dma = p; busy = 1;
}
static uint8_t expected(uint32_t pos)
{
    uint32_t offset = pos - FILEXFER_BMP_HEADER_LEN;
#if FILEXFER_STATIC_PATTERN
    offset %= 32768;
    return (uint8_t)offset ^ (uint8_t)((offset / 512) * 61);
#else
    uint32_t row = offset / (FILEXFER_IMG_WIDTH * 3);
    uint32_t col = (offset / 3) % FILEXFER_IMG_WIDTH;
    uint32_t actual_row = FILEXFER_IMG_HEIGHT - 1 - row;
    switch (offset % 3) {
    case 0: return (uint8_t)(col * 255 / (FILEXFER_IMG_WIDTH - 1));
    case 1: return (uint8_t)(actual_row * 255 / (FILEXFER_IMG_HEIGHT - 1));
    default: return (uint8_t)(col + actual_row);
    }
#endif
}
static void read_image(void)
{
    uint8_t snapshot[65536];
    unsigned packets = 0;
    uint32_t pos = 0;
    while (pos < FILEXFER_TOTAL_SIZE) {
        if (!busy) { FileXfer_Pump(); assert(busy); }
        assert(length && pos + length <= FILEXFER_TOTAL_SIZE);
        memcpy(snapshot, dma, length);
        /* An arbitrarily active producer must never overwrite DMA-owned rows. */
        if (packets % 3 == 0)
            for (unsigned i = 0; i < 100; i++) FileXfer_Pump();
        assert(!memcmp(snapshot, dma, length));
        if (pos == 0) assert(length == 54 && dma[0] == 'B' && dma[1] == 'M');
        for (unsigned i = 0; i < length; i++)
            if (pos + i >= FILEXFER_BMP_HEADER_LEN) assert(dma[i] == expected(pos + i));
        pos += length; packets++;
        busy = 0;
        /* Let USB drain ahead of main-loop generation to exercise backpressure. */
        USBD_EP2_StartTransfer();
    }
    assert(!busy);
    for (unsigned i = 0; i < 10; i++) FileXfer_Pump();
    assert(!busy);
}
int main(void)
{
    FileXfer_Init();
    FileXfer_Start(); FileXfer_Pump(); read_image();
    FileXfer_Start(); FileXfer_Pump();
    assert(busy && length == 54);
    FileXfer_Start(); FileXfer_Pump(); /* restart while a DMA is pending */
    read_image();
#if !FILEXFER_STATIC_PATTERN
    assert(starvation > 0);
#endif
    puts("file producer: correct data, backpressure, DMA ownership and restart passed");
}
