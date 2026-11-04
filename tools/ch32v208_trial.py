#!/usr/bin/env python3
"""CH32V208 entry point for the protected flash/readback/rollback runner."""
from ch585_trial import main


if __name__ == "__main__":
    raise SystemExit(main(default_chip="CH32V208"))
