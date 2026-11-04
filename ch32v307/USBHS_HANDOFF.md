# CH32V307 investigation paused — 2026-11-04

## Evening update — supersedes the earlier flashed-state note

The user identified the PCB as **CH32V307V-R2-1v1** and switched its power lead
to WCH-Link 3.3 V. On request, the **HS candidate** was subsequently flashed and
readback verified: `ch32v307/_build/safe512/app.bin`, 16192 bytes, SHA256
`d33689e2627ca20de818e6c22fbf32f8a7886d61f5b6632f3519237c1c00419e`.
Target UUID again matched `26-3e-97-2b-f5-a4-38-3b` (same MCU as the afternoon).
**Current state: HS firmware running, 3.3 V enabled, 5 V disabled.** Expected
device is now `1209:0001` on **HS**, not the earlier FS diagnostic PID.
It did not enumerate immediately after this flash. Readback is saved locally as
`ch32v307/_build/usbhs-session/evening-hs.readback.bin`.

The laptop USB-C port was observed as `device` / `sink`, receiving 5 V. Disabling
WCH-Link 5 V removed that Type-C partner and changed incoming voltage to zero.
This confirms the power direction, but exact cable/host-role routing remains to
be corrected; a sound alone is not USB enumeration. The R2 manual identifies
P9=FS, P11=HS and S2=power switch; it lists no USB data-routing jumper.

Historical successful V307 commits on 2026-08-30 record HS enumeration and file
verification, but do not record the PCB revision or MCU UUID. USB serial
`CH32V307-DEMO-0001` is hardcoded and cannot prove physical board identity.
The older scaffold's flash CS was PA2, not PE6; current demos do not initialize
the user's R2-specific PE6/PC6/PC13/PC14 peripheral idle/reset levels.

## Earlier pause snapshot

The user paused the task and requested saving all work to GitHub. **No V307
throughput improvement has been measured.** Neither the original HS firmware,
the prepared HS candidate nor the official FS diagnostic enumerated on this host.
Do not present cross-builds or producer tests as a successful USB hardware test.

**Physical state at pause:** official WCH **USB-FS diagnostic is flashed and
running**, product `CH32V307 USBFS DIAG`, expected VID/PID **1a86:5537** at 12M.
The HS candidate is not currently flashed. The user reported connecting FS.
WCH-Link's **3.3 V output is enabled**, explicitly requested by the user; 5 V
output was not enabled. No bootloader, option-byte or protection changes were made.

## Identified hardware and power

- WCH-LinkE 2.20, serial `F3388F06781D`, USB `1a86:8010`.
- Target **CH32V307**, UUID `26-3e-97-2b-f5-a4-38-3b`, part `30-70-05-18`.
- Reported flash **288 KiB**, read protection disabled; existing linker reserves
  **32 KiB SRAM**. Do not apply CH585's 32-row/large-burst profile to this layout.
- Initial identification failed with `nothing connected to linker` and invalid
  IDs. Enabling WCH-Link 3.3 V using **`minichlink -k3`** made identification work.
  `-k3` skips normal target initialization, which otherwise fails before a power
  command can run. Replugging WCH-Link may require enabling its output again.
- Tool: `/home/fiskov-v-b/Downloads/Prog/wch/ch32fun/minichlink/minichlink`.
- Use the probe lease **`WCH-LINK-F3388F06781D`**, normal priority, around the
  whole power/debug/program/test/restore transaction. Read the global mutex guide.

## Images and captured evidence

All original flash was backed up before programming. Durable local binaries are
under `backups/` (ignored), outside `_build`; see `backups/README.md` for hashes.

| Image | Bytes | Observation |
|---|---:|---|
| Original application flash | 294912 | Preserved before writes |
| `ch32v307/_build/baseline/app.bin` | 13388 | Source at `d9dc1d8`, readback matched, no HS enumeration |
| `ch32v307/_build/safe512/app.bin` | 16192 | Prepared 10 MiB candidate, readback matched; benchmark failed; baseline restored |
| `ch32v307/_build/fs-evt/usbfs-diag.bin` | 9520 | Official FS example, readback matched; **currently running**, no FS enumeration |

Paths above are relative to the repository root. Detailed trial output is committed in
`evidence/safe512-trial.json`; decoded register captures are in
`evidence/register-snapshots.json`. Remaining local captures/readbacks are under
`ch32v307/_build/usbhs-session/`.

Normal minichlink initialization disturbs/reset-halts the target: initial RCC
and USBHS dumps obtained that way **are not live application state**. Later
captures used `-kA` to halt without normal initialization, then resumed with `-e`:

```sh
minichlink -kA -s 0x17 0x002207b1 -m 0x04 -r <rcc.bin> 0x40021000 64 -e
```

- HS baseline DPC **0x934** resolved to `FileXfer_Pump` in the baseline ELF.
  USBHS CONTROL **0x31**: DMA, high-speed mode and device pull-up enabled;
  address/frame number remained zero. This does not prove PHY lock or wiring.
- FS diagnostic DPC **0xbb2** resolved to its main loop. FS BASE_CTRL **0x29**,
  UDEV_CTRL **0xa1**: controller/port and pull-up configured. Address stayed zero.
- Linux showed no successful target attachment/enumeration. Replug events were
  seen for the programmer/phone; no reliable target USB device was identified.
- User confirmed changing cable/HS connection, then moving to FS. Exact board
  model, connector routing and jumper positions were **not** established.

## Prepared changes, not hardware-validated

- EP2 DMA capacity changed from the old **64** to **512**, FS descriptor retains 64.
- Producer publishes ready rows after generation, advances only after completion,
  NAKs on starvation, aligns DMA storage and removes ad-hoc DATA-toggle resync.
- Gradient enlarged to **1024×3413, 10484790 bytes** (~10 MiB).
- Packed four-pixel/three-word generator; optional SRAM `.highcode` is copied in
  the existing startup data range. Four row buffers fit the existing RAM split.
- Default `-O3`, single-buffer reference mode, **INT_BUSY enabled**; double-buffer
  selection and ISR profiling are configurable experiments, not proven speedups.
- Fixed the old EP1 trigger dispatch/active-buffer overwrite path; added RX storage.
- Diagnostic 0x0C now returns `[EP2 completions, USB IRQs, profiled ISR us,
  SystemCoreClock, timer capture]` instead of the old idle/busy layout.
- Timer prescaler follows SystemCoreClock. Source config selects **96 MHz**,
  despite the old README/main comments claiming 144 MHz.

`make -C tools check` passes six CH585 and three V307 real-producer mock tests
with `-Werror` and UBSan, including full 10 MiB pattern/ownership/restart checks.
WCH GCC15 HS candidate cross-build passed: **16192 B flash, 24724 B / 32 KiB RAM**.
The SDK emits inherited warnings; SRAM code in `.data` also produces an RWX LOAD
segment linker warning. SRAM execution and actual USB transfers remain unverified.

## FS diagnostic reproduction and next experiment

Official EVT revision: **8b3699d9e4eafc3102e382b74266a1165a2bb584**,
`EVT/EXAM/USB/USBFS/DEVICE/CH372Device/User/`. The archive is stored at
`~/Desktop/work/datasheets/wch/CH32V307_EVT_8b3699d9e4ea.zip` with provenance.

```sh
# Genuine WCH GCC15 must be on PATH; no hardware access in these commands.
make -C ch32v307 -j8 BUILD_DIR=_build/resume
make -C tools check
python3 tools/build_v307_fs_diag.py --evt <CH32V307_EVT_8b3699d9e4ea.zip>
```

Default diagnostic changes only the official product string. **Important:** its
D8C USBFS clock still comes from the **HS USBPHY PLL**, so this FS test did not
fully isolate a shared clock problem. The builder has a prepared **`--clock pll`**
mode selecting the system PLL's divided 48 MHz clock without enabling USBHS:

```sh
python3 tools/build_v307_fs_diag.py --evt <archive.zip> --clock pll --build-dir ch32v307/_build/fs-pll
```

That alternate clock mode was **not built or flashed before the pause**; Python
syntax is checked only. It is a high-priority next experiment, alongside checking
the actual board schematic, VBUS/3.3 V routing, connector jumpers and data cable.
Do not conclude a cable fault solely from software-configured pull-ups.

Other follow-ups: validate HS/FS clocks against the actual crystal, review the
legacy HS EP1 OUT descriptor's 64-byte MPS, then measure safe 512-byte transfers
before experimenting with V307-specific double buffering. It has **no CH585
T_BURST register**. Do not copy that driver wholesale or change memory split options.

`tools/ch32v307_trial.py --resource WCH-LINK-F3388F06781D` reuses the protected
identity/readback/rollback runner, with V307 flash address **0x08000000** and this
board's UUID. It expects a working BMP interface, so it is **not** a test runner
for the official CH372 FS diagnostic (different PID and protocol).
