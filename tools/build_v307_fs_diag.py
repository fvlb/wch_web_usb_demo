#!/usr/bin/env python3
"""Build the official pinned V307 USBFS CH372 example as a connector diagnostic.

Default mode changes only the USB product string. The optional --clock=pll
mode also replaces USBFS clock initialization to isolate it from the HS PHY.
All extracted sources and images remain in the specified build directory.
"""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
REVISION = "8b3699d9e4eafc3102e382b74266a1165a2bb584"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evt", type=pathlib.Path, required=True)
    parser.add_argument("--build-dir", type=pathlib.Path,
                        default=ROOT / "ch32v307/_build/fs-evt")
    parser.add_argument("--cross", default="riscv32-wch-elf-")
    parser.add_argument("--clock", choices=("phy", "pll"), default="phy",
                        help="Use original USBPHY clock or isolate FS with system PLL / 48 MHz")
    args = parser.parse_args()
    destination = args.build_dir.resolve()
    user = destination / "User"
    user.mkdir(parents=True, exist_ok=True)
    prefix = "EVT/EXAM/USB/USBFS/DEVICE/CH372Device/User/"
    with zipfile.ZipFile(args.evt) as archive:
        metadata = json.loads(archive.read("UPSTREAM.json"))
        if metadata["revision"] != REVISION:
            raise ValueError("Unexpected EVT revision")
        for member in archive.namelist():
            if member.startswith(prefix) and member.endswith((".c", ".h")):
                relative = pathlib.PurePosixPath(member[len(prefix):])
                if len(relative.parts) != 1:
                    raise ValueError("Unexpected nested source path")
                (user / relative.name).write_bytes(archive.read(member))
    descriptors = user / "usb_desc.c"
    product = "CH32V307 USBFS DIAG" if args.clock == "phy" else "V307 USBFS PLL DIAG"
    encoded = bytes([2 + len(product) * 2, 3]) + product.encode("utf-16le")
    replacement = "const uint8_t MyProdInfo[] = {" + ",".join(hex(b) for b in encoded) + "};"
    contents, changes = re.subn(
        r"const\s+uint8_t\s+MyProdInfo\s*\[\s*\]\s*=\s*\{.*?\};",
        replacement, descriptors.read_text(), flags=re.S,
    )
    if changes != 1:
        raise ValueError("Product descriptor not found exactly once")
    descriptors.write_text(contents)
    if args.clock == "pll":
        driver = user / "ch32v30x_usbfs_device.c"
        body = """void USBFS_RCC_Init(void)
{
    RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_PLLCLK);
    if (SystemCoreClock == 144000000) RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div3);
    else if (SystemCoreClock == 96000000) RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div2);
    else if (SystemCoreClock == 48000000) RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div1);
    else while (1) {}
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);
}"""
        contents, changes = re.subn(r"void USBFS_RCC_Init\(void\)\s*\{.*?\n\}", body,
                                    driver.read_text(), flags=re.S)
        if changes != 1:
            raise ValueError("USBFS clock initializer not found exactly once")
        driver.write_text(contents)
    vendor = ROOT / "ch32v307/vendor"
    sources = sorted(user.glob("*.c")) + [vendor / "Debug/debug.c", vendor / "Core/core_riscv.c"]
    sources += sorted((vendor / "Peripheral/src").glob("*.c"))
    sources.append(vendor / "Startup/startup_ch32v30x_D8C.S")
    includes = [user, vendor / "Debug", vendor / "Core", vendor / "Peripheral/inc"]
    elf = destination / "usbfs-diag.elf"
    command = [args.cross + "gcc", "-march=rv32imafcxw", "-mabi=ilp32f", "-Os", "-g",
               "-std=gnu11", "-Wall", "-Wextra", "-DDEBUG=1", "-ffunction-sections",
               "-fdata-sections", "-nostartfiles", "--specs=nano.specs", "--specs=nosys.specs",
               "-Wl,--gc-sections,--print-memory-usage", f"-Wl,-Map,{destination / 'usbfs-diag.map'}",
               "-T" + str(vendor / "Ld/Link.ld"), *["-I" + str(p) for p in includes],
               *map(str, sources), "-o", str(elf)]
    subprocess.run(command, check=True)
    for extension, format_name in (("bin", "binary"), ("hex", "ihex")):
        subprocess.run([args.cross + "objcopy", "-O", format_name, str(elf),
                        str(destination / f"usbfs-diag.{extension}")], check=True)
    binary = destination / "usbfs-diag.bin"
    print(json.dumps({"binary": str(binary), "bytes": binary.stat().st_size,
                      "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                      "product": product, "clock": args.clock,
                      "vid_pid": "1a86:5537", "evt_revision": REVISION}))


if __name__ == "__main__":
    main()
