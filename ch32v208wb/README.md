# CH32V208WB USBFS demo — CH32F&V208W-R0-1v4

Board-specific port of the shared WebUSB/WinUSB LED and BMP test protocol.

## Wiring and detected device

- MCU detected: **CH32V208**, part `20-80-05-1c`, UUID `a7-d8-5b-e7-57-d3-54-14`.
- WCH-Link reports **160 KiB flash**; linker uses **160 KiB flash / 32 KiB RAM**.
- **J1 USB-C routes PB6=D−, PB7=D+**, the **USBFS/OTG** controller at 0x50000000.
  The PA11/PA12 USBD controller is a different block and does not drive J1.
- UART1: **PA9 TX / PA10 RX**, 115200 8N1. Both directions verified with `?`.
- **LED1 PD2**: SET_LED-controlled software PWM (64 duty steps; GET_LED retains
  the requested byte). **LED2 PD3**: 500 ms heartbeat.
- No button pin was specified: GET_BUTTON_RAW reports released; EP1 is reserved.
- External WCH-Link 3.3 V was used; 5 V output remained disabled. Keep debugger
  GND common and use the probe mutex resource `WCH-LINK-F3388F06781D`.

The exact PCB file in the official EVT archive confirms J1 and its nets.
R18/R19 are marked **NC (5.1K)** on CC2/CC1 to GND. If unpopulated, C-to-C may
not attach. The successful test used **USB-A host → A-to-C data cable → J1**.

## Working checkpoint, 2026-11-05

- WCH GCC15 build and flash readback passed.
- USB device **1209:0001**, product `CH32V208WB USBFS J1`, enumerates at **12 Mb/s**.
- UART startup reports **96 MHz**; HSE is **32 MHz**, CH32V20x_D8W startup/ABI.
- Seven **1024×3413, 10484790-byte (~10 MiB) gradient BMPs** across three flashes passed
  byte-by-byte verification: **0.896 MB/s**, about **11.703 seconds/file**, using
  16 × 64 KiB host reads. Each file used **163825 EP2 completion interrupts**.
- Physical controls passed both before and after that series: version, BOS,
  WinUSB descriptor, WebUSB URL, file size, LED brightness readback at
  0/64/128/255, advancing millisecond timer and IRQ/clock diagnostics.
  LED readback verifies the protocol; visible LED behavior was not independently observed.
- Portable-core tests pass with `-Werror` and UBSan: descriptors, control requests,
  complete images, DMA ownership, backpressure and restart while a DMA is pending.

**Bring-up fixes:** match WCH EVT's initialization order: enable controller/DMA
in BASE_CTRL before publishing endpoint DMA configuration. Also handle a SETUP
token directly on EP0, as EVT does: **do not reject SETUP based on the status
register's endpoint bits**. Nonzero bits were observed even for control traffic
(e.g. 2 after bulk IN). Filtering them caused intermittent enumeration and control
timeouts after the first file; removing that filter passed the series above.
The unmodified official CH372 reference independently enumerated on the same cable.

## Build and checks

Genuine WCH GCC15 (`riscv32-wch-elf-gcc`) is required for `rv32imacxw`, ILP32.
The pinned SDK is vendored, with notices and import details in `SDK_PROVENANCE.md`.

```sh
export PATH="/path/to/RISC-V Embedded GCC15/bin:$PATH"
make -C ch32v208wb -j8
make -C tools check
```

Artifacts: `ch32v208wb/_build/demo.{elf,bin,hex,map}`. Firmware currently occupies
about 14 KiB flash and 20 KiB SRAM. One inherited SDK warning remains in debug.c
for an unused fd parameter; application/core tests compile with warnings as errors.

To regenerate the SDK from the official snapshot saved in the WCH datasheets folder:

```sh
python3 tools/prepare_v208_sdk.py --evt <CH32V20X_EVT_b375bbbc7dfe.zip>
```

Do not rerun that import over intentional vendor edits without reviewing its diff.
Reference build: `make -C ch32v208wb REFERENCE=1` (official CH372, VID/PID 1a86:5537,
**not** the WebUSB/BMP application). Separate output directory preserves both images.

## Runtime tests and programming

Under the board/probe mutex:

```sh
ch585m/_build/tools/bmp-bench 16 65536 1
ch585m/_build/tools/v208-controls
```

`tools/ch32v208_trial.py` provides identity guard, protected programming/readback,
benchmark and rollback, using flash address **0x08000000**. `--flash-only` verifies
programming only and makes no USB success claim. It waits 12 seconds after boot
for host re-enumeration (override with `--settle-seconds`). Example from the repo root:

```sh
~/.local/bin/station-mutex run --owner WCH-CH32V208-USB --session <session> --priority normal WCH-LINK-F3388F06781D -- python3 tools/ch32v208_trial.py \
  --resource WCH-LINK-F3388F06781D --image ch32v208wb/_build/demo.bin \
  --fallback ch32v208wb/backups/original-reported-flash-2026-11-04.bin \
  --record ch32v208wb/_build/trial.json --minichlink <path-to-minichlink> \
  --bench ch585m/_build/tools/bmp-bench --runs 1
```

The original firmware is backed up both over the reported 160 KiB range and the
full 480 KiB mapped code area, including non-zero-wait storage. Binaries remain
local/ignored under `backups/`, outside build cleanup. No option/boot bytes changed.
The first captured old UART banner identified the previous application as firmware
v1000, built Jun 3 2026, Git `7cbdd00 refactor` (CAN/ADC/flash application).

The browser at https://fvlb.github.io/webusb-led/ uses the same protocol, including
GET_FILE_SIZE and exact final read sizing; it can be tested after native checks.
Detailed measurements and the tested binary hash are in [USBFS_REPORT.md](USBFS_REPORT.md).
