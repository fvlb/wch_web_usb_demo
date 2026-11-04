#!/usr/bin/env python3
"""CH32V307 entry point for the shared protected flash/readback/rollback runner."""
from ch585_trial import main


if __name__ == "__main__":
    raise SystemExit(main(default_chip="CH32V307"))
