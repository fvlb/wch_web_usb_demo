# CH585M USBHS throughput investigation — 2026-11-04

## Final state

The physical CH585M is running the optimized **gradient** firmware with a
**1024×3413, 24-bit BMP: 10,484,790 bytes**, approximately 10 MiB.

- **30/30** larger files verified byte-for-byte, **314,543,700 bytes** total.
- Transfer throughput: **25.623–26.799 MB/s**, median **26.1125 MB/s**.
- Aggregate bytes / aggregate USB transfer time: **26.1116 MB/s**.
- Each image transfers in about **0.40 seconds**. Verification time is excluded.
- Firmware: `ch585m/_build/final10m/app.bin`, 11624 bytes.
- SHA256: `50e824d40a3e10857b0dc240808e78ef2331abfad8f020e93f7effe4ca11cbfc`.
- Linker SRAM: **111980 / 131072 bytes**, including the large row ring and hot code.

This is a measured, reliable profile on this board, not proof that every host or
workload has reached an absolute chip limit. A static-pattern diagnostic isolated
the transport and achieved **43.17–43.38 MB/s**, without live gradient generation.

## Connection and preservation

- Probe: `1a86:8010`, WCH-LinkE firmware 2.20, serial `F3388F06781D`.
- Target detected over debug: **CH585**, flash **448 KiB**, UUID
  `df-44-84-88-19-70-7c-3d`, read protection disabled.
- Before firmware loading, only WCH-Link enumerated. Original application flash
  was backed up in full, then the existing demo was programmed and started.
- The demo enumerated as **1209:0001 at 480M** on USB path **3-1.1.1** through
  two Huasheng USB2 hubs. This confirms the **HS connector**, not the FS port.
- Host: Linux **7.0.0-42-generic**, xHCI, libusb **1.0.27**.
- Compiler: genuine WCH GCC15, `riscv32-wch-elf-gcc` 15.2.0, MounRiver 2.5.0.
- Original source baseline: `6b889a9e41330851134827f341da8932b9789a7c`.

Original flash SHA256:
`d5a268477fd17f812af9e70d1d3bc0efc2f0fb94ac7f3633becea150bb5d49e6`.
Local original/known-good backups are retained under `ch585m/backups/`, outside
build cleanup. Programming transactions used the board-specific mutex resource
`CH585-df44848819707c3d`, readback comparison and failure rollback.

## What was actually wrong

### 1. High-speed descriptor, full-speed-sized payloads

`ch585_usbhs_device.h` defined EP2 size 512, then included `usb_desc.h`, which
redefined it to 64. The latter value controlled the DMA transfer and fill limit,
while the HS descriptor still advertised 512. The compiler warned about this,
but the firmware built. A 3 MiB download produced **49152** short 64-byte payload
completions plus the header, not 6144 full 512-byte packets.

One pending host read reproduced **1.23–1.24 MB/s** with correct data. Queuing
more reads increased it to 2.43 MB/s but exposed stale row contents. There was
no evidence for the previous claim that the measured ceiling represented
~2000 full 512-byte packets per second.

### 2. Unpublished and DMA-owned rows

Increasing the packet limit alone reached 15.99 MB/s but corrupted the image.
The consumer did not check whether a row had been generated. Additionally,
`GenerateRow(s_genRow++)` could publish readiness before generation finished.
The queued-byte cursor could also free rows while a larger DMA was still using them.

The corrected pipeline publishes completed rows after a compiler barrier,
NAKs on producer starvation and advances its consumed cursor **after DMA
completion**, not on submission. A source row cannot be recycled while in flight.
The 54-byte header and all DMA storage are explicitly aligned.

### 3. Burst support was available

The local **`BLE/CH585DS1.PDF` V1.6 includes the register-level documentation**,
not just marketing specifications. Its USBHS chapter documents the bits needed:

| Register / field | Meaning used here |
|---|---|
| `R8_U2EP_T_BURST`, bit 2 | Enable EP2 transmit burst |
| `R8_U2EP_T_BURST_MODE`, bit 2 | Suppress automatic zero-length termination in burst mode |
| `R16_U2EP_T_TOG_AUTO`, bit 2 | Hardware data-toggle progression across the burst |
| `R32_U2EP2_MAX_LEN` | USB packet maximum, **512** at HS / **64** at FS |
| `R16_U2EP2_T_LEN` | Total DMA span, up to a 16-bit length |
| `R8_U2EP2_TX_CTRL`, bit 7 | Write-zero-to-clear DONE; release only after rearming |
| `R8_U2EP2_TX_CTRL`, bits 3:2 | DATA PID selection; manual changes are invalid in auto mode |
| `R8_U2EP2_TX_CTRL`, bit 6 | NAK completion flag, **not** the DATA toggle |

See printed datasheet pages **172–175** for these fields. CherryUSB's WCH USBHS
driver uses the same burst / no-ZLP / auto-toggle combination. The implementation
keeps DONE until pointer, total length and ACK are published, and stops manually
toggling DATA1 while automatic toggle is enabled.

The old main-loop “resync” that XORed `0x40` was removed: it changed a NAK status
flag, not the data toggle. Unused generic TX register base typos `0x404090xx`
were also corrected to the actual `0x400090xx` addresses.

### 4. Producer execution from flash was expensive

Burst alone did not eliminate the gradient-generation bottleneck. Moving the
generator to **`.highcode` SRAM** improved the 3 KiB-burst gradient from **2.69**
to **14.27 MB/s**. Compiler `-O3` in flash alone only reached 3.26 MB/s.

The packed generator writes four BGR pixels as three aligned 32-bit words,
reusing blue-lane templates and broadcasting green lanes. This preserves every
original gradient byte while reducing byte-store/loop overhead. Large ready
spans are transmitted directly, never copied into an intermediate USB buffer.

## Controlled experiment matrix

Unless noted, these comparisons used the earlier **3145782-byte (3 MiB) BMP**,
16 queued 65536-byte native host reads, three full downloads, and strict data
verification. MB/s is decimal. Timing includes START and initial prefill/read,
and ends at the last received byte; verification and flashing are outside it.

| Variant | MB/s | Correct? | Notes |
|---|---:|---|---|
| Original, queue 1 / 512-byte requests | 1.233–1.241 | yes | 49153 EP2 completions including header |
| Original, queue 16 | 2.432 | **no** | stale row data; not accepted as a speed result |
| Fix 512-byte payload only | 15.986 | **no** | producer corruption; automatic baseline rollback |
| 512-byte payload + ready-row guard | 2.565 | yes | generation backpressure exposed |
| 128-byte payload diagnostic | 2.147–2.150 | yes | HS descriptor remains 512 |
| 256-byte payload diagnostic | 2.391 | yes | no advantage over full packets |
| 3 KiB bursts, 8 rows, `-Os`, flash generator | 2.686 | yes | 1025 DMA completion IRQs |
| 3 KiB bursts, `-O3`, flash generator | 3.256 | yes | producer still slow |
| 3 KiB bursts, `-Os`, SRAM generator | 14.263–14.267 | yes | major execution-placement effect |
| 16 KiB bursts, 8 rows, `-O3`, SRAM | 16.830–16.899 | yes | byte-store generator |
| Packed generator, 16 KiB / 8 rows | 24.184–24.238 | yes | about 35 KiB SRAM |
| Packed generator, 32 KiB / 16 rows | 24.150–24.371 | yes | about 59 KiB SRAM |
| Packed generator, 65024 bytes / 32 rows | 25.354–26.253 | yes | selected throughput profile |
| Final 3 MiB, selected profile, **100 files** | 25.276–26.983 | **100/100** | aggregate 26.0714 MB/s |
| Current 10 MiB, selected profile, **30 files** | 25.623–26.799 | **30/30** | aggregate 26.1116 MB/s |
| Static pattern, 16 KiB burst | 43.166–43.378 | **5/5** | 193 DMA IRQs / 3 MiB file |
| Static pattern, 32 KiB burst | 43.188–43.350 | **5/5** | 97 DMA IRQs; no material speed gain |

Static pattern is a precomputed 32 KiB payload with a packet-dependent byte
pattern. Every byte is verified against its expected stream position, but the
pattern repeats every 32 KiB. It is a transport diagnostic, not the normal image
or a claim of cryptographic detection of every possible reorder.

Additional host checks on the final 3 MiB gradient profile:

| Host queue / request size | Verified MB/s |
|---|---:|
| 1 / 512 bytes | 6.556–7.045 |
| 1 / 65536 bytes | 25.936–26.397 |
| 32 / 262144 bytes | 25.475–26.041 |

Both firmware and host request sizing matter. The ~21× headline comparison
combines the repaired firmware and larger host requests. At the original
one-512-byte-request setting, firmware alone improves about 5–6×.

## Reproduce / inspect evidence

From repository root with WCH GCC15 on PATH:

```sh
make -C ch585m -j8 BUILD_DIR=_build/current
make -C tools check
ch585m/_build/tools/bmp-bench 16 65536 3
python3 tools/summarize_trials.py ch585m/_build/usbhs-session
```

Readback-verified programming and benchmark records are locally retained under
`ch585m/_build/usbhs-session/*.json`. They include command lines, image hashes,
return codes, captured output and rollback results; failed trials remain visible.
Generated binaries/logs are ignored. The original 448 KiB flash backup and the
verified final image are also preserved outside `_build`.

`make -C tools check` compiles the actual producer with a mocked endpoint/DMA
boundary under `-Wall -Wextra -Werror` and UBSan. Six source/buffer configurations
exercise full-file byte integrity, producer starvation, DMA ownership during
aggressive pumping, EOF and restart while a transfer is pending. These supplement
the physical tests; they do not emulate USB SIE/PHY timing. Full cross-builds
still emit pre-existing unused-function/vendor warnings; those are not hidden.

## References and Chinese-language research

- Official CH585 EVT, pinned `db889195f07b8f3f4e7bf2c55f96a35daa14bebf`:
  <https://github.com/openwch/ch585/tree/db889195f07b8f3f4e7bf2c55f96a35daa14bebf/EVT>.
- CherryUSB WCH USBHS port, pinned `1cf7193676584a0c2bbfa53bf88b13b65a063130`:
  <https://github.com/cherry-embedded/CherryUSB/blob/1cf7193676584a0c2bbfa53bf88b13b65a063130/port/wch/usbhs/usb_dc_usbhs.c>.
  Used as a register/ownership reference; this project still uses its own stack.
- Local `datasheets/wch/BLE/CH585DS1.PDF`, V1.6, USBHS register chapter.
- Chinese HS/FS configuration discussion:
  <https://www.cnblogs.com/azou/p/19875831>. Useful register-selection context;
  actual negotiated speed was verified with Linux, not inferred from bcdUSB.
- Chinese CH585 USBHS CDC example and DMA/completion discussion:
  <https://jishuzhan.net/article/1986788041906782210> (矜辰所致).
  Shows descriptor MPS, four-byte-aligned DMA buffers and completion handling;
  it does not establish a maximum throughput figure. Some general statements
  in the article, such as BOS being USB3-only, should not be treated as USB specs.
- Chinese hardware/crystal/debug reference:
  <https://www.cnblogs.com/iot-fan/p/13458627.html>.

Chinese searches also targeted the WCH community and 21ic forum. WCH's current
forum delivered only its JavaScript shell to the available reader, some search
requests timed out or were challenged, and no independently verifiable forum
maximum-speed claim was obtained. The performance numbers above are our own
measurements, not attributed to inaccessible forum posts.

The requested official EVT folders are archived under
`/home/fiskov-v-b/Desktop/work/datasheets/wch/` as
`CH585_EVT_db889195f07b.zip` and `CH32V307_EVT_8b3699d9e4ea.zip`, with SHA256
sidecars, CRC checks, internal `UPSTREAM.json` and `EVT_ARCHIVES.md` provenance.
