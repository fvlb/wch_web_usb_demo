# wch-usb-demos

WinUSB / WebUSB LED + file-transfer demo firmwares for several WCH RISC-V
MCUs, all speaking the **identical USB protocol** (0x1209:0x0001) so a
single web page can drive any board:

- [`ch32l103/`](ch32l103) — WCH CH32L103 (QingKe V4C). LED on PB8
  (hardware TIM4_CH3 PWM), button on PA1.
- [`ch592f/`](ch592f) — WCH CH592F. LED on PB23 (TMR0 hardware PWM,
  remapped), button on PB22 (BOOT pin). Zero-copy DMA, interrupt-driven
  UART TX, CPU profiling (vendor 0x0C).
- [`ch585m/`](ch585m) — WCH CH585M. LED on PA9 (TMR0 hardware PWM),
  UART on PA14. USBHS burst DMA, packed SRAM row generation, **~26 MB/s**
  byte-verified gradient download; default BMP is **~10 MiB**.
- [`ch32v307/`](ch32v307) — WCH CH32V307 (QingKe V4F). USBHS high-speed,
  zero-copy rows, DMA UART TX, CPU profiling (vendor 0x0C). LED pin TBD.
- [`ch32v208wb/`](ch32v208wb) — CH32F&V208W-R0-1v4, CH32V208WB.
  USBFS on **J1 PB6/PB7**, UART PA9/PA10, LED1 PD2 / heartbeat PD3.
  **0.896 MB/s** byte-verified ~10 MiB gradients; seven files across three flashes.

Common device protocol:

- Advertise a BOS descriptor with **Microsoft OS 2.0** (Windows
  auto-binds `winusb.sys`, no `.inf` file) and **WebUSB** (browser
  `GET_URL` landing-page) Platform Capabilities side by side.
- Expose LED brightness control (`SET_LED`/`GET_LED`) and a synthetic BMP
  streamed over EP2 bulk IN (~10 MiB on CH585M and V208; query request 0x09).
  Boards with a wired button also report it over EP1 interrupt IN; V208
  reserves EP1 and reports released because no button pin was specified.
- Use VID/PID **0x1209 / 0x0001** (the open ["pid.codes" test
  allocation](https://pid.codes/1209/0001/)).

See each project's own README for hardware wiring, the full vendor-request
table, and build/flash instructions:

- [`ch32l103/README.md`](ch32l103/README.md)
- [`ch592f/README.md`](ch592f/README.md)
- [`ch32v208wb/README.md`](ch32v208wb/README.md)

## Latest verified measurements

Both tested firmwares generate the same **1024×3413-pixel, 10,484,790-byte
gradient BMP**. Results use 64 KiB host reads and full byte-by-byte verification.

| Board | USB link | Verified throughput | Verification | Date |
|---|---|---:|---|---|
| CH585M | High-speed, 480 Mb/s | **25.62–26.80 MB/s** | 30 complete files | 2026-11-04 |
| CH32V208WB | Full-speed, 12 Mb/s | **0.896 MB/s** | 7 complete files across 3 flashes | 2026-11-05 |

CH32V307's current candidate still awaits successful hardware enumeration and
transfer validation. CH32L103 and CH592F have not been remeasured in this campaign.

Implementation details, earlier comparisons and diagnostic workloads:

- [CH585 USBHS report](ch585m/USBHS_REPORT.md)
- [CH32V208WB USBFS report](ch32v208wb/USBFS_REPORT.md)
- [CH32V307 bring-up status](ch32v307/USBHS_HANDOFF.md)

```sh
make -C tools check
ch585m/_build/tools/bmp-bench 16 65536 3
```

## Web page

The browser-side control page is **not** part of this repository. It is
built and hosted from the separate
[`fiskov.github.io`](https://github.com/fiskov/fiskov.github.io) repository
and works with either board, since both speak the same protocol:

- `https://fiskov.github.io/webusb-led/index.html` (one page for both
  boards, since the protocol is identical)

## Building

Each firmware is a fully independent Make project; build them separately
from their own directory:

```sh
export PATH="/path/to/RISC-V Embedded GCC15/bin:$PATH"

cd ch32l103 && make -j16   # -> ch32l103/_build/winusb_webusb_led.{elf,hex,bin}
cd ../ch592f && make -j16  # -> ch592f/_build/app.{elf,hex,bin}
cd ../ch585m && make -j16  # -> ch585m/_build/app.{elf,hex,bin}
```

See [`.vscode/tasks.json`](.vscode/tasks.json) for ready-made VS Code build
tasks covering both projects.
