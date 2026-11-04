# V307 bring-up evidence

- `safe512-trial.json`: captured MCU identification, protected programming,
  byte-for-byte readback, failed USB benchmark and successful baseline restore.
- `register-snapshots.json`: debugger-halted peripheral snapshots, with the
  distinction between normal programmer initialization (reset-contaminated)
  and `-kA` (halt without normal initialization). See the handoff for interpretation.

These are failure/investigation records, **not throughput results**. The final
physical state is a subsequently flashed FS diagnostic. Original flash and
built binaries remain local, with hashes documented in `../backups/README.md`.
