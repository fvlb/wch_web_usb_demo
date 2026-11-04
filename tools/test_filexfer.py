#!/usr/bin/env python3
"""Exercise the actual CH585 producer with a mocked USB/DMA boundary."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    variants = [
        (0, 4, 0, 0), (3072, 8, 0, 0), (16384, 8, 1, 0),
        (32768, 16, 1, 0), (65024, 32, 1, 0), (16384, 4, 0, 1),
    ]
    with tempfile.TemporaryDirectory() as tmp:
        for burst, rows, packed, pattern in variants:
            binary = str(pathlib.Path(tmp) / "filexfer-test")
            command = [
                "cc", "-std=gnu11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined", "-fno-sanitize-recover=all",
                "-include", "ch585m/tests/filexfer_mock.h", "-Ich585m/app",
                f"-DUSBHS_EP2_BURST_BYTES={burst}", f"-DFILEXFER_ROW_BUF_CNT={rows}",
                f"-DFILEXFER_PACKED_GENERATOR={packed}", f"-DFILEXFER_STATIC_PATTERN={pattern}",
                "ch585m/tests/filexfer_test.c", "ch585m/app/filexfer.c", "-o", binary,
            ]
            print(f"burst={burst}, rows={rows}, packed={packed}, static={pattern}", flush=True)
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run([binary], check=True)


if __name__ == "__main__":
    main()
