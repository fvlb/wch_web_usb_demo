# CH32V208WB verified USBFS checkpoint — 2026-11-05

## Target and artifact

- Board: CH32F&V208W-R0-1v4; CH32V208 UUID `a7-d8-5b-e7-57-d3-54-14`.
- Host: Linux/xHCI; USB-A → A-to-C cable → J1, PB6/PB7 USBFS, **12 Mb/s**.
- WCH-LinkE 2.20 serial `F3388F06781D`; target supplied at 3.3 V, probe 5 V off.
- Firmware: `ch32v208wb/_build/demo.bin`, **13968 bytes**.
- SHA256: `e8f42afc81bf929a106800615a9b1de63c68d7b2c7f67e1ecbcf9bf57c0abc29`.
- WCH GCC15 15.2.0, `rv32imacxw` / ILP32, `-O3`; HSE32 → SYSCLK96 / USB48.
- Linked SRAM: 19560 bytes, including reserved stack; 160 KiB flash / 32 KiB RAM.
- Pinned SDK: openwch/ch32v20x `b375bbbc7dfe8642d6fb7c17e6d3207ee1c3c9c1`.
- Local official archive: `CH32V20X_EVT_b375bbbc7dfe.zip`, SHA256
  `6bcf73468f41bc45bb5af76063340b75826503507324308579ef6581fe78fe77`.

## Verified transfers

The native libusb harness queued **16 × 65536-byte reads**. Every test queried
the file size, consumed the initial 54-byte BMP header, capped requests at EOF,
and verified the complete gradient byte by byte. Each file is **10484790 bytes**,
1024×3413 pixels. Timing ends at final USB completion, before host verification.

| Flash session | Run | Seconds | MB/s | Byte verification |
|---|---:|---:|---:|---|
| SETUP fix | 1 | 11.702990 | 0.896 | PASS |
| SETUP fix | 2 | 11.703253 | 0.896 | PASS |
| SETUP fix | 3 | 11.703015 | 0.896 | PASS |
| Reflash | 1 | 11.703390 | 0.896 | PASS |
| Reflash | 2 | 11.703075 | 0.896 | PASS |
| Reflash | 3 | 11.703234 | 0.896 | PASS |
| Final trial helper | 1 | 11.703157 | 0.896 | PASS |

Each file produced **163825 EP2 completion IRQs** and a measured delta of
**163830 total USB IRQs**. The 160 host async completions are 64 KiB-sized
host transfers, not USB packets. No HS/burst claim applies to this USBFS MCU.

`v208-controls` passed before and after both three-file series: firmware 2.0.8,
57-byte BOS, 30-byte MS OS/WinUSB descriptor, WebUSB URL, file size, LED brightness
readback at 0/64/128/255 (original restored), advancing timer, IRQ count and 96 MHz
core report. UART `?` also passed after streaming, reporting configured=1 and
491475 EP2 completions. Visible LED behavior and a browser UI session were not
independently observed in these native tests.

Flash readback matched on each session. The final trial helper returned success
after its identity guard, programming/readback, 12-second boot wait and verified
file. Full local command records/readbacks are ignored under `ch32v208wb/_build/`
(`setup-token-flash.json`, `final-flash.json`, `final-trial.json`).

## Bring-up findings

1. Exact board PCB routes J1 to **PB6/PB7 USBFS**, not PA11/PA12 USBD. The pinned
   unmodified WCH CH372 reference enumerated as 1a86:5537, independently proving
   this board/cable path.
2. Match EVT's init order: enable BASE_CTRL DMA/controller before endpoint state.
   That change first allowed enumeration and one verified file, but did not by
   itself establish reliability.
3. **Handle SETUP directly on EP0, like EVT.** Filtering its endpoint bits dropped
   valid requests: status could report nonzero bits (notably 2 after EP2 traffic).
   Symptoms included enumeration timeouts and GET_STATS/next START timeouts after
   one valid image. Removing that filter enabled all seven files and control checks.
4. Timer-disabled, software-saved-ISR and plain-rv32imac diagnostic builds were
   explored before isolating the SETUP filter. Their partial results did not
   establish a timer/toolchain fault. The final firmware retains both fast ISRs
   and the original WCH ISA flags.
5. Allow host re-enumeration after debug programming. The trial helper now waits
   12 seconds on V208 (configurable); a one-second wait could report no device
   before Linux had finished re-enumerating.

## Local checks

- `make -C tools check`: six CH585 producer profiles, three V307 profiles and
  V208 real-core tests with UBSan and warnings as errors passed.
- V208 firmware build passed; the imported SDK debug `_write` has an inherited
  unused-parameter warning. Application and host-tool warning checks passed.
- The official reference build also passed, with its inherited main.c `%d` versus
  uint32_t format warning in addition to the debug warning; it is not warning-clean.
- Original 160 KiB and mapped 480 KiB backups were captured before programming;
  see `backups/README.md`. Boot/option/read-protection bytes were not modified.

The board is left running the tested WebUSB/BMP demo, with LED brightness restored
to zero and the PD3 heartbeat active in firmware.
