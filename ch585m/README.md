# CH585M WinUSB / WebUSB LED + USBHS file transfer

Original vendor protocol and LED/button functions, with a generated 24-bit BMP
stream on EP2 bulk IN. The current image is **1024×3413 pixels, 10,484,790 bytes**
(approximately 10 MiB). Query vendor request **0x09** for its size; **0x0A** starts
or restarts a transfer. The first short packet is the 54-byte BMP header.

## Connected board and results

Use the **HS** connector for this firmware, not FS. The tested board enumerates
as `1209:0001` at **480 Mb/s**. WCH-Link itself enumerates separately at 12 Mb/s;
that does not indicate the MCU's USB data speed.

Measured on the demoboard, 2026-11-04:

- Current 10 MiB gradient: **25.62–26.80 MB/s**, all **30** complete files verified.
- Earlier 3 MiB version: **25.28–26.98 MB/s**, all **100** complete files verified.
- Diagnostic precomputed pattern: **43.17–43.38 MB/s**, verified; not the default.
- Original code, 64-byte short packets, one host read pending: **1.23–1.24 MB/s**.

Full experiment matrix, failures, datasheet bits and memory costs are in
[`USBHS_REPORT.md`](USBHS_REPORT.md).

## Build

Use genuine **WCH GCC15**, not generic xPack GCC15 (it does not support `xw`).
From the repository root:

```sh
export PATH="/path/to/RISC-V Embedded GCC15/bin:$PATH"
make -C ch585m -j8 BUILD_DIR=_build/current
make -C tools check
```

Outputs: `ch585m/_build/current/app.{elf,bin,hex,map}`. The already tested 10 MiB
image from this campaign is at `ch585m/_build/final10m/app.{bin,hex}`.

Default build settings select the highest verified gradient-throughput profile:

| Setting | Default |
|---|---:|
| `USBHS_EP2_BURST_BYTES` | 65024 |
| `USBHS_EP2_PAYLOAD_LIMIT` | 512 |
| `FILEXFER_ROW_BUF_CNT` | 32 |
| `FILEXFER_RAM_GENERATOR` | 1 |
| `FILEXFER_PACKED_GENERATOR` | 1 |
| `FILEXFER_STATIC_PATTERN` | 0 |
| `OPT` | `-O3 -Wextra` |

The 10 MiB build uses **111980 / 131072 bytes SRAM** and **11624 bytes flash**.
For less SRAM, use 8 rows and a 16384-byte burst; that configuration measured
about 24.2 MB/s with the earlier 3 MiB file and used about 35 KiB SRAM.

Use a fresh `BUILD_DIR` when changing compiler flags; the legacy Make rules do
not track flag changes as dependencies. `make clean` deletes the selected
build directory. Original/known-good firmware backups live outside `_build`.

## Test

```sh
# Linux dependencies: build-essential, pkg-config, libusb-1.0-0-dev, Python 3
make -C tools
ch585m/_build/tools/bmp-bench 16 65536 3
```

The host submits asynchronous reads, reconstructs stream order, and checks every
gradient byte. Its JSON reports decimal MB/s and actual host completion lengths.
Verification happens after transfer timing. Kernel/library versions and USB
topology are recorded in the report. Run physical access under the shared mutex
as shown in `AGENTS.md`; the simple invocation above illustrates tool arguments.

Browser/native clients should request **64 KiB** chunks, query the file length,
continue after the initial 54-byte short header, and request only the remaining
bytes for the final read. Sending one host request per USB packet can limit
throughput even with fast firmware. The externally hosted page is a separate repo.

For deliberate 128-/256-byte payload diagnostics, disable burst mode and set
`USBHS_EP2_PAYLOAD_LIMIT`; keep the HS endpoint descriptor MPS at 512.

## Flash and recovery

The user's working programmer is `minichlink` from `ch32fun`, with WCH-LinkE 2.20.
The experiment runner checks the chip UUID, programs, reads back and compares the
binary, boots, runs the benchmark, and restores a supplied known-good binary if
the trial fails. See `AGENTS.md` for the complete command.

Preserve `backups/original-flash-2026-11-04.bin` (448 KiB) and
`backups/verified-usbhs-10m-2026-11-04.bin` (tested fast image); binaries are ignored.
Do not erase/change option bytes or bootloader settings for throughput work.
