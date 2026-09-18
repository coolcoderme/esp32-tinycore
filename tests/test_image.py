#!/usr/bin/env python3
"""Check that MicroCore-ESP32P4.img is an MBR + FAT32 LBA volume Rufus can flash."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_image.py MicroCore-ESP32P4.img", file=sys.stderr)
        return 2
    img = Path(sys.argv[1])
    if not img.is_file():
        print(f"missing {img}", file=sys.stderr)
        return 1

    env = os.environ.copy()
    env["MTOOLS_SKIP_CHECK"] = "1"
    env["PATH"] = "/usr/sbin:/sbin:" + env.get("PATH", "")

    out = subprocess.check_output(["sfdisk", "-d", str(img)], text=True)
    if "type=c" not in out and "type=0C" not in out and 'type="c"' not in out:
        print("expected FAT32 LBA (type=c) partition:\n", out, file=sys.stderr)
        return 1
    if "bootable" not in out:
        print("expected bootable flag:\n", out, file=sys.stderr)
        return 1

    vol = subprocess.check_output(
        ["file", "-b", str(img)], text=True
    ).strip()
    print("file(1):", vol)

    listing = subprocess.check_output(
        ["mdir", "-i", f"{img}@@1048576", "::/boot"],
        env=env,
        text=True,
    )
    listing_l = listing.lower()
    for token in ("loader", "vmlinuz", "core", "esp32p4", "readme"):
        if token not in listing_l:
            print(f"missing /boot/{token} in:\n{listing}", file=sys.stderr)
            return 1
    print("image ok:", img, f"({img.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
