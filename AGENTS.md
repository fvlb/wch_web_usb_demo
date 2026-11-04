# AI development handoff — WCH USB demo

## Current work and scope

This is the original `fvlb/wch_web_usb_demo` Git repository, not the adjacent
`wch-usb-lab`. The user explicitly returned here to improve CH585 throughput on
the connected demoboard, test multiple approaches and compare CherryUSB. Read
`ch585m/README.md` and `ch585m/USBHS_REPORT.md` before continuing.

CH585 now defaults to a **1024×3413, 10484790-byte (~10 MiB) gradient BMP**.
The final firmware is **already flashed** and running on the board. Thirty full
10 MiB files passed at **25.6–26.8 MB/s**; the prior 3 MiB firmware passed 100
files. A deliberately different static pattern reached ~43.3 MB/s. Never report
the static result as live gradient throughput. CH32V307 and other MCU code was
not changed or revalidated by this campaign.

## Physical board, programmer and ownership

- CH585 UUID: `df-44-84-88-19-70-7c-3d`, 448 KiB flash.
- WCH-LinkE 2.20: USB `1a86:8010`, serial `F3388F06781D`.
- Target data interface: `1209:0001`, **480M**, USB path `3-1.1.1` through two hubs.
- The board has separate **FS** and **HS** connectors. Use **HS**. WCH-Link's own
  12M enumeration is unrelated to target USB speed.
- Programmer executable currently available at
  `/home/fiskov-v-b/Downloads/Prog/wch/ch32fun/minichlink/minichlink`.
- **Even `minichlink --help` initializes debug access and can halt/reset the MCU.**
  Read its README/source rather than querying help on an active target.

Read `~/.local/share/jotunn/station-mutex.md` before physical access. Use this
board's own resource, **not** the unrelated charging-station resource:

```sh
~/.local/bin/station-mutex run --owner WCH-CH585-USBHS --session <unique-session> --priority normal CH585-df44848819707c3d -- <whole-transaction>
```

Example complete trial, from this repository root:

```sh
~/.local/bin/station-mutex run --owner WCH-CH585-USBHS --session <unique-session> --priority normal CH585-df44848819707c3d -- python3 tools/ch585_trial.py \
  --image ch585m/_build/current/app.bin \
  --fallback ch585m/backups/verified-usbhs-10m-2026-11-04.bin \
  --record ch585m/_build/usbhs-session/new-trial.json \
  --minichlink /home/fiskov-v-b/Downloads/Prog/wch/ch32fun/minichlink/minichlink \
  --bench ch585m/_build/tools/bmp-bench --queue 16 --runs 3
```

The script requires a lease, checks MCU/UUID, protects programming+readback,
compares the readback, boots, measures and rolls back on failure. Supply a real,
previously verified fallback. Do not change bootloader/option/read-protection
settings, power other hardware, or open an unrelated serial port for this work.

## Durable backups and tested artifacts

- `ch585m/backups/original-flash-2026-11-04.bin`: original 458752-byte application
  flash, SHA256 `d5a268477fd17f812af9e70d1d3bc0efc2f0fb94ac7f3633becea150bb5d49e6`.
- `ch585m/backups/verified-usbhs-10m-2026-11-04.bin`: final tested 11624-byte image,
  SHA256 `50e824d40a3e10857b0dc240808e78ef2331abfad8f020e93f7effe4ca11cbfc`.
- `ch585m/_build/final10m/app.{bin,hex,elf,map}`: current built artifacts.
- `ch585m/_build/usbhs-session/`: trial JSON, intermediate images/readbacks, logs.

Backup binaries are ignored and outside build cleanup. Do not overwrite them.
Unchanged SDK/main code still has compiler warnings; no claim of a clean full
SDK lint/build is made. The actual producer tests and host tool use `-Werror`.

## Build/check

Use genuine WCH GCC15 (`riscv32-wch-elf-gcc` 15.2.0), not generic xPack GCC15.
The local MounRiver installer is under `~/Downloads/Prog/wch/`; the initial
session extracted its compiler into a temporary `.ya/tmp/...` directory. Put
the actual compiler `bin` on PATH; do not rely on that extraction surviving.

```sh
make -C ch585m -j8 BUILD_DIR=_build/current
make -C tools check
git diff --check
```

Use a fresh BUILD_DIR after changing flags: legacy Make rules do not encode
compiler flags as object dependencies. Normal defaults select -O3, SRAM/packed
generation, 32 row buffers and a 65024-byte maximum burst. For a lower SRAM
profile: `FILEXFER_ROW_BUF_CNT=8 USBHS_EP2_BURST_BYTES=16384`.

## Proven invariants

1. EP2 DMA capacity is 512, FS descriptor MPS 64, HS descriptor MPS 512. Never
   restore the conflicting header redefinitions that silently made HS send 64.
2. `s_genPos` is a **completed** cursor. The fill callback accounts for the
   preceding DMA before choosing the next span. Do not free rows on submission.
3. Publish `s_genRow` only after GenerateRow and a compiler memory barrier;
   consumers NAK instead of reading unprepared rows. No generation in the USB IRQ.
4. DMA storage/pointers are four-byte aligned. A burst cannot cross the physical
   end of the row ring, enter ungenerated rows or overwrite in-flight data.
5. Burst: T_BURST + T_BURST_MODE + T_TOG_AUTO. Keep MAX_LEN at actual packet MPS,
   set T_LEN to the complete span, and do not manually toggle DATA in auto mode.
6. DONE is released after DMA/length/ACK publishing. CTRL bit 6 (`0x40`) is a NAK
   flag, not a DATA-toggle resynchronization mechanism.
7. The local CH585DS1 PDF **contains full register documentation**; read USBHS
   printed pages 172–175. Do not assume a separate RM is required to see burst bits.
8. Host requests should be large (64 KiB), file size queried through 0x09, and
   the final read limited to remaining data. The initial 54-byte short packet
   is a header, not EOF. The external browser UI is a different repository.
9. `g_ep2Count` counts completion IRQs, not individual packets once burst is on.
10. Do not conflate queued USB transport speed, full-file verification time,
    firmware flashing, raw/static-pattern speed, or untested theoretical limits.

## References and Git handoff

CherryUSB was used as a register/ownership reference, revision
`1cf7193676584a0c2bbfa53bf88b13b65a063130`, `port/wch/usbhs/usb_dc_usbhs.c`.
This repository still uses its own original USB stack. CherryUSB alternatives
live separately in `../wch-usb-lab`; do not overwrite that repository's pending work.

Official EVT snapshots requested by the user are in
`~/Desktop/work/datasheets/wch/`, with ZIP/SHA256/provenance; see `EVT_ARCHIVES.md`.
Chinese-language sources and forum-access limitations are documented in the report.

Baseline was `6b889a9e41330851134827f341da8932b9789a7c`. After the local campaign,
the user requested committing and pushing these changes. Check actual Git status
and remote state before continuing; commit/push further work only on request.
