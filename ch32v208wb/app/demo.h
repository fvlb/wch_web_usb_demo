#ifndef V208_USB_DEMO_H
#define V208_USB_DEMO_H
#include <stdint.h>
#include <stddef.h>

#define DEMO_WIDTH 1024u
#define DEMO_HEIGHT 3413u
#define DEMO_HEADER_BYTES 54u
#define DEMO_ROW_BYTES (DEMO_WIDTH * 3u)
#define DEMO_FILE_BYTES (DEMO_HEADER_BYTES + DEMO_ROW_BYTES * DEMO_HEIGHT)

extern uint8_t demo_ep0[64];
extern volatile uint8_t demo_brightness;
extern volatile uint8_t demo_configured;
extern volatile uint32_t demo_usb_irqs;

void demo_init(void);
void demo_poll(void);
void demo_usb_reset(void);
void demo_usb_setup(void);
void demo_usb_in(unsigned endpoint);
void demo_usb_out(unsigned endpoint, unsigned length);

void board_ep0_tx(const uint8_t *data, unsigned length, int data1);
void board_ep0_rx(int data1);
void board_ep0_stall(void);
void board_bulk_tx(const uint8_t *data, unsigned length, int data1);
void board_bulk_stop(void);
void board_address(uint8_t address);
void board_endpoints(void);
uint32_t board_millis(void);
uint32_t board_core_clock(void);
const char *board_serial(void);
#endif
