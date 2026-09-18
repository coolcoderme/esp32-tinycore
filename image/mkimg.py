#!/usr/bin/env python3
"""Build a Rufus/BalenaEtcher-flashable MicroCore disk image for ESP32-P4.

Creates an MBR disk with one bootable FAT32 LBA partition (type 0x0C),
volume label MICROCORE, and the TinyCore-style frugal layout:

    /boot/loader.cfg  /boot/vmlinuz  /boot/core.gz  /boot/esp32p4.dtb
    /tce/onboot.lst   /home          /opt
"""
from __future__ import annotations

import argparse
import gzip
import io
import os
import stat
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PART_START = 2048  # 1 MiB alignment
SECTOR = 512
MIN_FREE = 12 * 1024 * 1024
PSRAM_32 = 32 * 1024 * 1024
PSRAM_64 = 64 * 1024 * 1024


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    print("+", " ".join(cmd))
    return subprocess.run(cmd, check=True, **kw)


def write_riscv_image(path: Path, payload: bytes) -> None:
    hdr = bytearray(64)
    struct.pack_into("<I", hdr, 0, 0x0000006F)  # jal zero, 0
    size = 64 + len(payload)
    struct.pack_into("<Q", hdr, 16, size)
    struct.pack_into("<Q", hdr, 48, 0x5643534952)  # "RISCV"
    struct.pack_into("<I", hdr, 56, 0x05435352)  # magic2
    note = payload if payload else b"ESP32-P4 MicroCore placeholder kernel\n"
    path.write_bytes(bytes(hdr) + note)


def cpio_newc(entries: list[tuple[str, bytes, int]]) -> bytes:
    out = bytearray()
    ino = 1

    def add(name: str, data: bytes, mode: int) -> None:
        nonlocal ino
        name_b = name.encode("ascii") + b"\0"
        namesize = len(name_b)
        hdr = ("070701" + f"{ino:08x}{mode:08x}{0:08x}{0:08x}{1:08x}{0:08x}"
               f"{len(data):08x}{0:08x}{0:08x}{0:08x}{0:08x}{namesize:08x}{0:08x}").encode("ascii")
        out.extend(hdr)
        out.extend(name_b)
        while len(out) % 4:
            out.append(0)
        out.extend(data)
        while len(out) % 4:
            out.append(0)
        ino += 1

    for name, data, mode in entries:
        add(name.lstrip("/"), data, mode)
    add("TRAILER!!!", b"", 0)
    return bytes(out)


def write_core_gz(path: Path) -> None:
    init = b"""#!/bin/sh
echo "ESP32-P4 MicroCore placeholder init"
echo "Rebuild core.gz with Buildroot (see linux/README.md)."
exec /bin/sh
"""
    cpio = cpio_newc(
        [
            ("init", init, 0o100755),
        ]
    )
    with gzip.GzipFile(filename="", mode="wb", fileobj=path.open("wb"), mtime=0) as z:
        z.write(cpio)


def compile_dtb(dts: Path, dtb: Path) -> None:
    run(["dtc", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(dts)])


def make_loader_cfg(path: Path) -> None:
    path.write_text(
        """default microcore
timeout 20

label microcore
  kernel /boot/vmlinuz
  initrd /boot/core.gz
  fdt    /boot/esp32p4.dtb
  append console=ttyS0,115200n8 earlycon rdinit=/init loglevel=4
""",
        encoding="utf-8",
    )


def make_readme(path: Path) -> None:
    path.write_text(
        """ESP32-P4 MicroCore (RISC-V)
==========================

This is NOT the x86 TinyCore/MicroCore ISO from tinycorelinux.net.

Flash this .img with Rufus (DD / Image mode) or BalenaEtcher, then insert
the card into the ESP32-P4 SD slot. The chip must already have the
linux-loader flashed to SPI flash.

Serial: 115200 8N1 on UART0.

Layout:
  /boot   kernel, core.gz, device tree, loader.cfg
  /tce    squashfs extensions (onboot.lst)
  /home   persistent extra storage (visible from Windows)
  /opt    optional persist

Drop files here from your PC; Linux mounts this same FAT volume at /mnt/sd.
""",
        encoding="utf-8",
    )


def fat_offset_bytes() -> int:
    return PART_START * SECTOR


def mtools_img(img: Path) -> str:
    return f"{img}@@{fat_offset_bytes()}"


def mtools(cmd: list[str], img: Path) -> None:
    env = os.environ.copy()
    env["MTOOLS_SKIP_CHECK"] = "1"
    run(cmd, env=env)


def check_budget(kernel: Path, initrd: Path, dtb: Path) -> None:
    used = kernel.stat().st_size + initrd.stat().st_size + dtb.stat().st_size
    print(f"payloads: {used} bytes ({used / (1024 * 1024):.2f} MiB)")
    for name, ram in (("32MB", PSRAM_32), ("64MB", PSRAM_64)):
        free = ram - used
        ok = free >= MIN_FREE
        print(f"  {name}: free {free / (1024 * 1024):.1f} MiB  {'OK' if ok else 'TOO SMALL'}")
        if name == "32MB" and not ok:
            print(
                "warning: 32 MB boards will refuse to boot this image "
                "(need 12 MiB free RAM after kernel+initrd+dtb)",
                file=sys.stderr,
            )
        if name == "64MB" and not ok:
            raise SystemExit("payloads do not fit in 64 MB PSRAM with 12 MiB free")


def main() -> int:
    os.environ["PATH"] = "/usr/sbin:/sbin:" + os.environ.get("PATH", "")
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-o", "--output", type=Path, default=Path("MicroCore-ESP32P4.img"))
    ap.add_argument("--size-mb", type=int, default=256, help="image size in MiB")
    ap.add_argument("--kernel", type=Path, help="RV32 Image (named vmlinuz on the card)")
    ap.add_argument("--initrd", type=Path, help="core.gz initramfs")
    ap.add_argument("--dtb", type=Path, help="compiled esp32p4.dtb")
    ap.add_argument("--dts", type=Path, default=REPO / "dts" / "esp32p4-microcore-standalone.dts")
    args = ap.parse_args()

    if args.size_mb < 64:
        raise SystemExit("size-mb must be >= 64 so the volume is FAT32")

    out = args.output.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="microcore-img-") as td:
        td_path = Path(td)
        kernel = args.kernel
        initrd = args.initrd
        dtb = args.dtb
        if kernel is None:
            kernel = td_path / "vmlinuz"
            write_riscv_image(kernel, b"placeholder RISC-V Image for ESP32-P4 MicroCore\n")
            print(f"note: using placeholder kernel {kernel}")
        if initrd is None:
            initrd = td_path / "core.gz"
            write_core_gz(initrd)
            print(f"note: using placeholder core.gz {initrd}")
        if dtb is None:
            dtb = td_path / "esp32p4.dtb"
            compile_dtb(args.dts, dtb)

        check_budget(kernel, initrd, dtb)

        loader_cfg = td_path / "loader.cfg"
        readme = td_path / "README.TXT"
        onboot = td_path / "onboot.lst"
        hello = td_path / "hello.txt"
        make_loader_cfg(loader_cfg)
        make_readme(readme)
        onboot.write_text("# extensions to load at boot, one .tcz name per line\n", encoding="utf-8")
        hello.write_text("persistent extra storage on the MicroCore SD card\n", encoding="utf-8")

        # Sparse disk image + MBR.
        if out.exists():
            out.unlink()
        run(["truncate", "-s", f"{args.size_mb}M", str(out)])
        sfdisk_in = "label: dos\nunit: sectors\n\nstart=2048, type=c, bootable\n"
        run(["sfdisk", "--no-reread", str(out)], input=sfdisk_in.encode())

        # FAT32 inside the partition. Hidden sectors = partition start.
        part_sectors = (args.size_mb * 1024 * 1024) // SECTOR - PART_START
        run(
            [
                "mkfs.vfat",
                "-F",
                "32",
                "-n",
                "MICROCORE",
                "-s",
                "4",
                "-h",
                str(PART_START),
                f"--offset={PART_START}",
                str(out),
                str(part_sectors // 2),  # mkfs.vfat BLOCKS are 1 KiB
            ]
        )

        img = mtools_img(out)
        for d in ("::/boot", "::/tce", "::/home", "::/opt"):
            mtools(["mmd", "-i", img, d], out)
        copies = [
            (loader_cfg, "::/boot/loader.cfg"),
            (kernel, "::/boot/vmlinuz"),
            (initrd, "::/boot/core.gz"),
            (dtb, "::/boot/esp32p4.dtb"),
            (readme, "::/boot/README.TXT"),
            (onboot, "::/tce/onboot.lst"),
            (hello, "::/home/hello.txt"),
        ]
        for src, dest in copies:
            mtools(["mcopy", "-i", img, str(src), dest], out)

    print(f"wrote {out} ({args.size_mb} MiB, MBR + FAT32 LBA, label MICROCORE)")
    print("Flash with Rufus (DD / Image mode) or BalenaEtcher.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as e:
        raise SystemExit(e.returncode) from e
