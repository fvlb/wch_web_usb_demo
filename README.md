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

## CH585M: verified USBHS improvement, 2026-11-04

The connected board was identified through WCH-Link and confirmed at **480M**
on its HS port. The original firmware actually submitted **64-byte short
packets** despite advertising a 512-byte HS bulk endpoint. After correcting
that mismatch, producer/DMA ownership, burst mode and SRAM generation:

| Workload | Verified throughput | Verification |
|---|---:|---|
| Original 3 MiB gradient, one 512-byte host read pending | 1.23–1.24 MB/s | 2 full files |
| Optimized 3 MiB gradient, 64 KiB host reads | 25.28–26.98 MB/s | 100 full files |
| **Current 10 MiB gradient**, 64 KiB host reads | **25.62–26.80 MB/s** | **30 full files** |
| Precomputed diagnostic pattern, burst DMA | 43.17–43.38 MB/s | 10 full files across 2 burst sizes |

The static-pattern result isolates USB transport; it is not gradient generation
speed. The default remains the original gradient, now **1024×3413 pixels,
10,484,790 bytes**. Use large host reads; a serial stream of 512-byte host reads
still adds substantial scheduling overhead. Details, register references,
failed experiments and build commands: [CH585 USBHS report](ch585m/USBHS_REPORT.md).

```sh
make -C tools check
ch585m/_build/tools/bmp-bench 16 65536 3
```

## Historical measurements (earlier shared harness)

The earlier figures below used the same Linux/xHCI host and synthetic 3 MiB
gradient BMP. The CH585M row is historical, superseded by the measurements above:

| Board    | USB         | Architecture                  | Throughput | ISR/packet | CPU in ISR |
|----------|-------------|-------------------------------|-----------:|-----------:|-----------:|
| CH32L103 | full-speed  | ring buffer + memcpy           | ~230 KB/s | —          | —          |
| CH592F   | full-speed  | **zero-copy DMA from ring**    | **287 KB/s** | **7.8 µs** | **3.6%** |
| CH585M   | high-speed  | zero-copy rotating rows        | ~1000 KB/s | ~1 µs      | ~2%        |
| CH32V307 | high-speed  | zero-copy rotating rows        | ~1000 KB/s | 1.2 µs     | 1.9%      |

Notes:

- Full-speed theoretical max: ~1.2 MB/s (19 × 64-byte transactions/frame).
  The CH592F at 287 KB/s reaches ~24% of this (limited by xHCI NAK-backoff
  with INT_BUSY; removing INT_BUSY doubles speed but corrupts data).
- High-speed theoretical max: ~53.25 MB/s. The former attribution of the CH585M
  ~1 MB/s result solely to a 512-byte-per-IRQ limit was incorrect: it was sending
  64-byte short packets and also had producer/ownership and flash execution
  bottlenecks. The CH32V307 has not been remeasured in this campaign.
- CPU in ISR measured via TMR3/TIM3 at 1 µs resolution (vendor request 0x0C
  on CH592F and CH32V307).
- The zero-copy DMA on CH592F uses the same ring buffer as the stock code
  but the DMA reads directly from the ring slot (no memcpy). One slot of
  headroom prevents the pump from overwriting a slot the DMA is reading.

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
