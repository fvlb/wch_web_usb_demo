# Local CH32V307 backup inventory — 2026-11-04

These binary files are retained locally and ignored by Git. They are outside
`_build` so normal build cleanup does not remove the original flash backup.
Do not overwrite them. None is claimed to be a USB-working image in this session:
flash/readback succeeded, but USB enumeration remained blocked.

Target: CH32V307, UUID `26-3e-97-2b-f5-a4-38-3b`, 288 KiB reported flash.

| File | Bytes | SHA256 |
|---|---:|---|
| `original-flash-2026-11-04.bin` | 294912 | `4d266aaaff9f15a88ebcb5cf7a8b7819cc5cd6f51df696f6ccdf9cbc54bfa8c0` |
| `baseline-hs-2026-11-04.bin` | 13388 | `2ea3e025f4110b50f7a79445afb5cd45e193acc887f47ac6890e024e187a2aff` |
| `candidate-hs-10m-2026-11-04.bin` | 16192 | `d33689e2627ca20de818e6c22fbf32f8a7886d61f5b6632f3519237c1c00419e` |
| `running-fs-diag-2026-11-04.bin` | 9520 | `f3a01ea870ff375f03c22047f4949e033a81e67b5aed289a815d3fff7d2816a1` |

**At pause, `running-fs-diag-2026-11-04.bin` is flashed**, not the HS candidate.
It is the official EVT CH372 USBFS example with a changed product string.
Reproduction instructions, physical state and unresolved issues are in
[`../USBHS_HANDOFF.md`](../USBHS_HANDOFF.md). Binary hashes and source references
are committed; the full unknown original flash image is not published.
