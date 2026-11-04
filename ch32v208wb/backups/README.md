# CH32V208WB local backups

Target identified as CH32V208, UUID `a7-d8-5b-e7-57-d3-54-14`, part `20-80-05-1c`.
WCH-Link reports a 160 KiB flash configuration. The datasheet also maps code
flash up to 480 KiB, including the non-zero-wait area. Backups are taken before
programming; binary files are ignored and kept outside `_build`.

| File | Bytes | SHA256 |
|---|---:|---|
| `original-reported-flash-2026-11-04.bin` | 163840 | `1ae21b1de169648256263325cc3626969bc299fd6a01cd8cb3d362d6071344bf` |
| `original-code-flash-480k-2026-11-04.bin` | 491520 | `78fdfdb30a0c592d746ec4322b4cc14098f4a31269b342a744a196a23f8e0e57` |

Both reads completed successfully before the first write. The reported-range
backup is sufficient to restore the boot/application region changed by this demo.
Retain the complete mapped-range backup as well; never overwrite either file.
