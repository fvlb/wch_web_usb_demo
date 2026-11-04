# Local demoboard backup

`original-flash-2026-11-04.bin` is a locally retained, ignored 448 KiB application
flash backup taken **before** this throughput campaign programmed the board.

- MCU: CH585, UUID `df-44-84-88-19-70-7c-3d`
- Bytes: 458752
- SHA256: `d5a268477fd17f812af9e70d1d3bc0efc2f0fb94ac7f3633becea150bb5d49e6`
- Read with minichlink through WCH-LinkE 2.20; read protection was disabled.

The binary is not committed. This directory is outside `_build` so normal build
cleanup cannot erase the only original-firmware backup. Do not overwrite it.

`verified-usbhs-10m-2026-11-04.bin` is the final working 10 MiB-gradient firmware,
11624 bytes, SHA256
`50e824d40a3e10857b0dc240808e78ef2331abfad8f020e93f7effe4ca11cbfc`.
It passed flash readback and thirty complete image verifications on this board.
