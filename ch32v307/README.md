# CH32V307 WinUSB / WebUSB LED demo (USBHS high-speed)

> **Investigation paused, 2026-11-04:** the prepared 512-byte/packed-generator
> candidate builds and passes producer tests, but neither HS nor the official
> FS diagnostic enumerated. No new V307 throughput result exists. The board is
> currently running the FS diagnostic, not this candidate. Read
> **[USBHS_HANDOFF.md](USBHS_HANDOFF.md)** for exact state, evidence and next steps.
>
> The candidate now generates **1024×3413 / 10484790-byte (~10 MiB)** images.

CH32V307 port of [`../ch585m`](../ch585m): the same USB descriptors
(0x1209:0001), vendor protocol and fully zero-copy EP2 bulk path
(pointer packets into four rotating gradient rows) on the USBHS
high-speed peripheral.

- QingKe V4F (`rv32imafcxw`/`ilp32f`), startup_ch32v30x_D8C.S; current source selects **96 MHz**
- systick: TIM2 1 kHz (same `SysTick_Millis()` API)
- debug UART via the EVT `USART_Printf_Init(115200)`
- LED: plain GPIO heartbeat; **pin is a TODO** in `led_pwm.c`
  (`LED_PORT`/`LED_PIN`, currently PA1) - board LED not identified yet
- button on PB1 (unpopulated placeholder)
- USBD_Device_Init() calls USBHS_RCC_Init() + USBHS_Device_Init(ENABLE)
  (the EVT main called the RCC/PLL setup separately - easy to miss)

Historical reports recorded ~1000 KB/s. They are not a measurement of this
candidate or proof of a V307 hardware limit; its old implementation sent
64-byte payloads despite the 512-byte HS descriptor.

Build & flash:

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16
minichlink -w _build/app.bin 0x08000000
```
