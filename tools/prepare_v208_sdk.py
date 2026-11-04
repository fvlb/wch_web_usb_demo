#!/usr/bin/env python3
"""Populate the V208 port from the pinned official EVT snapshot (offline)."""
import argparse
import json
import pathlib
import re
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
REVISION = "b375bbbc7dfe8642d6fb7c17e6d3207ee1c3c9c1"


def write_source(target, data):
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = data.decode("gb18030")
    lines = []
    for line in text.splitlines():
        body = line.lstrip(" \t")
        indent = line[:len(line) - len(body)].expandtabs(4)
        lines.append((indent + body).rstrip())
    target.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evt", type=pathlib.Path, required=True)
    args = parser.parse_args()
    board = ROOT / "ch32v208wb"
    with zipfile.ZipFile(args.evt) as archive:
        provenance = json.loads(archive.read("UPSTREAM.json"))
        if provenance["revision"] != REVISION:
            raise ValueError("Unexpected SDK revision")
        prefix = "EVT/EXAM/SRC/"
        for member in archive.namelist():
            if member.startswith(prefix) and member.endswith((".c", ".h", ".S", ".ld")):
                relative = pathlib.PurePosixPath(member[len(prefix):])
                if ".." in relative.parts:
                    raise ValueError("Invalid archive path")
                target = board / "vendor" / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                write_source(target, archive.read(member))
        example = "EVT/EXAM/USB/USBFS/DEVICE/CH372Device/User/"
        for member in archive.namelist():
            if member.startswith(example) and member.endswith((".c", ".h")):
                relative = pathlib.PurePosixPath(member[len(example):])
                if len(relative.parts) != 1:
                    raise ValueError("Unexpected nested reference source")
                target = board / "vendor/usbfs-reference" / relative.name
                target.parent.mkdir(parents=True, exist_ok=True)
                write_source(target, archive.read(member))
        for name in ("system_ch32v20x.c", "system_ch32v20x.h", "ch32v20x_conf.h"):
            target = board / "app" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            write_source(target, archive.read(example + name))
    linker = board / "vendor/Ld/Link.ld"
    contents, count = re.subn(r"MEMORY\s*\{.*?\}",
                             "MEMORY\n{\n  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 160K\n"
                             "  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 32K\n}",
                             linker.read_text(), count=1, flags=re.S)
    if count != 1:
        raise ValueError("Linker memory map not found")
    linker.write_text(contents)
    (board / "SDK_PROVENANCE.md").write_text(
        "# WCH SDK provenance\n\nOfficial openwch/ch32v20x EVT revision `" + REVISION + "`.\n"
        "Vendor SRC and the USBFS CH372 example clock/config sources are retained with WCH notices.\n"
        "Imported text is normalized to UTF-8/LF, space indentation and no trailing whitespace/blank EOF lines; source semantics are unchanged.\n"
        "The linker MEMORY map is selected as 160 KiB flash / 32 KiB SRAM, matching the detected split.\n"
        "No boot/option bytes are changed. Regenerate with `tools/prepare_v208_sdk.py --evt <archive>`.\n"
    )
    print("Prepared CH32V208WB SDK and 160K/32K linker map")


if __name__ == "__main__":
    main()
