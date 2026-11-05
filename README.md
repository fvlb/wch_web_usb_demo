# WCH WinUSB / WebUSB demos

LED control and generated BMP downloads over USB for five WCH RISC-V MCUs.
The firmwares share VID/PID **`1209:0001`** and a common control/file-transfer
protocol, so the same browser page or native client can drive each port.

## MCU comparison

Memory figures below are the **application flash and SRAM configured by this
repository's linker scripts**, not firmware binary sizes or free memory.

| MCU / firmware | Flash (KiB) | SRAM (KiB) | USB peripheral used | USB link rate | BMP throughput |
|---|---:|---:|---|---:|---:|
| [CH32L103](ch32l103/README.md) | 64 | 20 | USB full-speed | 12 Mb/s | — |
| [CH592F](ch592f/README.md) | 448 | 26 | USB full-speed | 12 Mb/s | — |
| [CH585M](ch585m/README.md) | 448 | 128 | USBHS high-speed | 480 Mb/s | **25.62–26.80 MB/s** |
| [CH32V208WB](ch32v208wb/README.md) | 160 | 32 | USBFS full-speed, J1 | 12 Mb/s | **0.896 MB/s** |
| [CH32V307](ch32v307/USBHS_HANDOFF.md) | 288 | 32 | USBHS high-speed candidate | 480 Mb/s | Pending bring-up |

- **USB link rate** is the peripheral's signaling rate; **BMP throughput** is
  captured application throughput in decimal MB/s. `—` means no current result.
- CH585M and CH32V208WB results use the same **1024×3413-pixel, 10,484,790-byte
  gradient BMP** and 64 KiB host reads, captured in November 2026.
- CH585M and CH592F each have **512 KiB total flash**, including **448 KiB
  CodeFlash**, 32 KiB DataFlash, 24 KiB bootloader and 8 KiB configuration storage.
- CH32V208 supports configurable flash/SRAM splits: **128/64**, **144/48** or
  **160/32 KiB**; this port uses **160/32 KiB**. CH32V307 also has a configurable
  split; its current linker selects **288/32 KiB**, rather than the common
  **256/64 KiB** configuration.

Memory maps: [CH32L103](ch32l103/Ld/Link.ld),
[CH592F](ch592f/vendor/Ld/Link.ld), [CH585M](ch585m/vendor/Ld/Link.ld),
[CH32V208WB](ch32v208wb/vendor/Ld/Link.ld), [CH32V307](ch32v307/vendor/Ld/Link.ld).

## Hardware and wiring

| Port | USB connection | LED control | Button |
|---|---|---|---|
| CH32L103 | Full-speed connector; see board schematic | PB8, TIM4_CH3 PWM | PA1 |
| CH592F | Full-speed connector | PB23, remapped TMR0 PWM | PB22 / BOOT |
| CH585M | **HS connector**, not FS | PA9, TMR0 PWM | PB22 |
| CH32V208WB | **J1, PB6 D− / PB7 D+** on CH32F&V208W-R0-1v4 | PD2 software PWM; PD3 heartbeat | Not assigned |
| CH32V307 | HS connector; see [bring-up notes](ch32v307/USBHS_HANDOFF.md) | Board LED mapping pending | PB1 placeholder |

On CH32V208WB, the debug UART is **PA9 TX / PA10 RX, 115200 baud**. J1 is wired
to the USBFS controller, not the separate PA11/PA12 USBD block. The current setup
uses a USB-A-to-C data cable; see its [board notes](ch32v208wb/README.md) for CC wiring.

## Browser demo

Open **[fvlb.github.io/webusb-led](https://fvlb.github.io/webusb-led/)** in a
WebUSB-capable browser, connect the board's USB data port, and select the demo
device. The page controls LED brightness and downloads the generated BMP.

The page lives in the separate
[`fvlb.github.io`](https://github.com/fvlb/fvlb.github.io) repository. Firmware
advertises Microsoft OS 2.0 / WinUSB and WebUSB platform capabilities in its BOS
descriptor. The demo uses the [pid.codes test VID/PID](https://pid.codes/1209/0001/).

## Build

Use **WCH GCC15** from MounRiver Studio, with `riscv32-wch-elf-*` on `PATH`.
Several ports use WCH's `xw` instructions, which generic RISC-V toolchains may
not support. Each directory is an independent Make project; choose a target:

```sh
export PATH="/path/to/RISC-V Embedded GCC15/bin:$PATH"

make -C ch32l103 -j8
make -C ch592f -j8
make -C ch585m -j8
make -C ch32v208wb -j8
make -C ch32v307 -j8
```

| Port | Default firmware output |
|---|---|
| CH32L103 | `ch32l103/_build/winusb_webusb_led.{elf,hex,bin}` |
| CH592F | `ch592f/_build/app.{elf,hex,bin}` |
| CH585M | `ch585m/_build/app.{elf,hex,bin}` |
| CH32V208WB | `ch32v208wb/_build/demo.{elf,hex,bin}` |
| CH32V307 | `ch32v307/_build/app.{elf,hex,bin}` |

Board-specific programming, backup and recovery instructions are in the linked
port READMEs. For CH585M and CH32V307, use a fresh `BUILD_DIR` when changing
compiler flags. Existing [VS Code tasks](.vscode/tasks.json) cover CH32L103 and CH592F.

## Native throughput tool

The shared host tool requires a C compiler, `pkg-config` and libusb development
headers (`libusb-1.0-0-dev` on Debian/Ubuntu). Connect one demo device at a time:

```sh
make -C tools
ch585m/_build/tools/bmp-bench 16 65536 3
```

Arguments are **queue depth**, **read size in bytes**, and **number of downloads**.
The output reports decimal MB/s. Despite its output directory, the tool is shared
across MCU ports. Transfer timing ends at the final USB completion.

## USB protocol

| Endpoint | Purpose |
|---|---|
| EP0 control | Descriptors, LED brightness, version, timer and transfer commands |
| EP1 interrupt IN | Button events on ports with a wired button; reserved on V208 |
| EP2 bulk IN | Generated 24-bit BMP stream |

Query **`GET_FILE_SIZE` (`0x09`)**, send **`START_FILE_XFER` (`0x0A`)**, then read
EP2 until the reported size is received. The initial **54-byte short packet is
the BMP header**, not EOF. Use large reads and cap the final read at the remaining
bytes. V208 reports its unassigned button as released; diagnostic request `0x0C`
has port-specific counters. Full request tables are in the port documentation.

## Implementation notes

- [CH585M: burst DMA and SRAM gradient generation](ch585m/USBHS_REPORT.md)
- [CH32V208WB: USBFS initialization and SETUP handling](ch32v208wb/USBFS_REPORT.md)
- [CH32V307: current bring-up status](ch32v307/USBHS_HANDOFF.md)
