/********************************** (C) COPYRIGHT *******************************
 * File Name          : filexfer.h
 * Description        : Synthetic file generator for USB bulk-transfer
 *                       throughput testing. Generates an uncompressed BMP
 *                       image on the fly (no data is stored in flash/RAM),
 *                       byte-for-byte reproducible on the host side for
 *                       correctness verification. Identical interface to
 *                       ../../ch32l103/User/filexfer.[ch].
 *******************************************************************************/
#ifndef APP_FILEXFER_H_
#define APP_FILEXFER_H_

#include <stdint.h>

/* Image dimensions of the synthetic BMP (24-bit, uncompressed).
 * 1024x3413x3 = 10484736 pixel bytes + 54-byte header = ~10 MiB total - a
 * "several megabytes" payload as requested, large enough for a
 * meaningful throughput measurement. Row size (1024*3 = 3072) is already
 * a multiple of 4, so no BMP row padding is needed. */
#define FILEXFER_IMG_WIDTH    1024u
#define FILEXFER_IMG_HEIGHT   3413u
#define FILEXFER_BMP_HEADER_LEN 54u
#define FILEXFER_TOTAL_SIZE   (FILEXFER_BMP_HEADER_LEN + (FILEXFER_IMG_WIDTH * FILEXFER_IMG_HEIGHT * 3u))

/* Registers the EP2 bulk-IN fill callback with the USB device stack.
 * Call once at startup. */
void FileXfer_Init(void);

/* Resets the read offset to 0 and (re)arms EP2 to begin streaming the
 * synthetic file from the beginning. Call in response to the
 * START_FILE_TRANSFER vendor request. */
void FileXfer_Start(void);

/* Fills the internal packet ring buffer with freshly-generated packets,
 * ahead of when the USB interrupt handler needs them. Call this as often
 * as possible from the idle main loop - it is cheap and returns
 * immediately once the ring is full or the file is exhausted. */
void FileXfer_Pump(void);

#endif /* APP_FILEXFER_H_ */
