# Plan: ESP32-P4 bootloader for MicroCore Linux on SD

**Implementation status:** the repository now contains the linux-loader
ESP-IDF project, portable FAT/MBR/cfg/DTB/layout libraries with host
tests (`make test`), a Rufus/Etcher FAT32 image builder
(`python3 image/mkimg.py`), MicroCore `/init` + `tce-load` overlay, and
device trees. A real RV32 kernel still needs an ESP32-P4 Linux port
(see `linux/README.md`); `mkimg.py` will pack a placeholder Image until
then.

This document is the implementation plan for `esp32-tinycore`. It is
intentionally concrete: what lives in flash, what lives on the SD card,
how Rufus/BalenaEtcher fit, and which constraints are non-negotiable.

## Goal

Ship a small bootloader that lives in the ESP32-P4's SPI flash and boots
a MicroCore-style Linux userspace whose kernel, initramfs, extensions,
and extra storage all live on the onboard microSD card.

The user workflow is:

1. Flash the bootloader to the ESP32-P4 once (`esptool`).
2. Write the MicroCore disk image to a microSD card with **Rufus** or
   **BalenaEtcher**.
3. Insert the card, reset the board, get a serial login.

## Hard constraints

These are physical or ISA facts, not preferences.

| Constraint | Consequence |
|---|---|
| ESP32-P4 HP cores are **RV32IMAFC**, typically run Linux in **M-mode with no MMU** | Stock TinyCore/MicroCore (x86 `bzImage` + glibc/i486 userland) cannot execute. There is no official RISC-V TinyCore. |
| ESP32 ROM only boots a signed ESP image from SPI flash | The ROM cannot chainload syslinux/isolinux from SD. Our bootloader must live in flash and *read* the SD card itself. |
| PSRAM is mapped through cache/MMU at `0x48000000`, up to **64 MB** of virtual address space | Kernel, DTB, initrd, and Linux RAM all share that window. The bootloader must init PSRAM *before* copying anything. |
| Official `Core-*.iso` from tinycorelinux.net is x86 | Rufus/Etcher of that ISO onto the card will not boot. We ship a RISC-V MicroCore *image* that those same tools can flash. |
| FLAT/static NOMMU userland | Dynamic ELF as used on x86 TinyCore does not apply. BusyBox and `.tcz` payloads must be RV32 NOMMU binaries. |

Emulating x86 MicroCore on the P4 is out of scope. Native RV32 Linux is
the only path that fits 64 MB and a 360 MHz core.

## What "MicroCore" means here

Keep TinyCore's **frugal** model, not its ISA:

- Kernel file + `core.gz` initramfs on the boot media.
- RAM-based root (tmpfs/initramfs), not a scattered disk install.
- Optional squashfs extensions under `/tce`.
- Persistent extra storage on the same FAT volume (home, ssh keys, extra
  packages) that Windows can still see after flashing.

The shipped userspace is a BusyBox MicroCore analogue built with
Buildroot, not a remaster of `Core-current.iso`.

## Hardware target

Primary target: **ESP32-P4 with onboard microSD (SDMMC 4-bit) and 64 MB
external PSRAM**.

Most retail modules are **ESP32-P4NRW32** (32 MB stacked HEX PSRAM).
Espressif documents a **64 MB virtual PSRAM window** and ESP-PSRAM64.
The bootloader will:

1. Initialize PSRAM via ESP-IDF (`CONFIG_SPIRAM_BOOT_INIT`, HEX/Octal).
2. Query the detected size.
3. Patch the DTB `memory` node to the real size.
4. Refuse to boot if kernel + initrd + minimum RAM do not fit.

Board profiles (Kconfig / `sdkconfig.defaults.<board>`):

| Profile | SDMMC | PSRAM assumption | Notes |
|---|---|---|---|
| `esp32_p4_function_ev` (default pin map) | Slot 0, 4-bit: CLK=GPIO43, CMD=GPIO44, D0–D3=GPIO39–42, LDO channel 4 | 32 MB typical, 64 MB if fitted | Espressif Function EV Board |
| `waveshare_esp32_p4` | SDIO 3.0 TF slot (board-specific overlay) | 32 MB stacked on current Waveshare kits | Confirm pins per SKU at bring-up |
| `esp32_p4_psram64` | Same SDMMC as chosen board | 64 MB (ESP-PSRAM64 / Octal) | Explicit 64 MB sdkconfig |

Console: UART0 at 115200. USB Serial/JTAG is a later nicety, not required
for first boot.

## Boot chain

```
Power-on
  ESP32 ROM
    -> 1st-stage IDF bootloader  (flash @ 0x2000)
         clocks, cache, flash MMU
    -> 2nd-stage "linux-loader"  (flash @ 0x10000)
         1. Init 32/64 MB PSRAM (MEMMAP, not heap)
         2. Init SDMMC, wait for card
         3. Parse MBR/GPT, then FAT32 (primary) or ISO9660 (fallback)
         4. Read /boot/loader.cfg (or TinyCore-style isolinux.cfg)
         5. Copy kernel Image  -> PSRAM
         6. Copy DTB           -> PSRAM (or use flash-baked DTB)
         7. Copy core.gz       -> PSRAM as initrd
         8. Patch DTB: memory size, initrd-start/end, bootargs, chosen
         9. PMP: unlock PSRAM window for M-mode Linux
        10. Cache writeback + I-cache invalidate + fence.i
        11. a0 = hartid, a1 = dtb, jump to kernel entry
    -> Linux RV32 NOMMU in PSRAM
         unpack core.gz, mount SD at /mnt/sd, tce-setup, getty
```

The 2nd-stage loader is an ESP-IDF app, not a replacement for the ROM
or the 1st-stage IDF bootloader. Proven pattern: why2025-linux's
`linux-native` shim, except the payload comes from SD instead of flash
partitions.

If the SD card is missing, unreadable, or contains an x86 `bzImage`,
print a clear UART error and sit in a recovery loop (watchdog-friendly
heartbeat so `esptool` can still reflash).

## SPI flash layout (bootloader only)

Keep flash small. Linux does **not** live here.

| Offset   | Size  | Content |
|----------|-------|---------|
| `0x0000` | 8 KB  | ROM boot header / chip boot |
| `0x2000` | 32 KB | ESP-IDF 1st-stage bootloader |
| `0x8000` | 4 KB  | Partition table |
| `0x9000` | 24 KB | NVS |
| `0xF000` | 4 KB  | PHY |
| `0x10000`| 1 MB  | linux-loader app |
| remainder|       | Optional recovery DTB / banner; unused |

No kernel partition. No squashfs-in-flash. That is the point of SD boot.

## SD card image (Rufus / BalenaEtcher)

Ship a raw `.img` (and optionally a matching `.iso`). Both tools write
raw images in DD mode. FAT32 is required so:

- the flash-resident loader can parse it with a small FAT driver
- Windows users can drop extra `.tcz` files and documents after flashing
- Linux can remount the same volume for `/tce` and extra storage

### Partition table

MBR, one primary partition, type `0x0C` (FAT32 LBA), boot flag set.
Volume label: `MICROCORE`.

Do **not** use ext4 as the first partition: Rufus/Etcher users on Windows
would get an unreadable card, and the bootloader would need an ext driver.

Image size: 256 MB sparse-friendly raw image (enough for kernel +
`core.gz` + a starter `/tce` plus headroom). The rest of a larger
physical card remains unpartitioned until the user expands the FAT
partition in Windows/macOS/Linux; a later `tools/expand-fat.sh` note
covers that.

### Filesystem layout

```
/boot/loader.cfg          # our boot config (authoritative)
/boot/vmlinuz             # RV32 Linux Image (TinyCore name, RISC-V bytes)
/boot/core.gz             # gzip+cpio initramfs
/boot/esp32p4.dtb         # optional; loader may use a built-in DTB
/boot/README.txt          # human: this is RISC-V MicroCore, not x86 Core.iso
/tce/                     # TinyCore-style extension dir (writable)
/tce/onboot.lst           # extensions to load at boot
/home/                    # persistent extra storage
/opt/                     # optional persist (TinyCore mydata analogue)
```

`loader.cfg` (TinyCore/syslinux-shaped on purpose):

```
default microcore
timeout 20

label microcore
  kernel /boot/vmlinuz
  initrd /boot/core.gz
  fdt    /boot/esp32p4.dtb
  append console=ttyS0,115200n8 earlycon rdinit=/init loglevel=4
```

If `loader.cfg` is missing, the loader also understands a TinyCore
`/boot/isolinux/isolinux.cfg` with `kernel` / `initrd` / `append`. That
lets a future remastered ISO keep familiar paths.

### What Rufus/Etcher users actually flash

**Correct:** `MicroCore-ESP32P4-<ver>.img` from this project's releases.

**Incorrect:** `Core-17.x.iso` / `TinyCore-*.iso` from tinycorelinux.net.

The loader must detect the x86 kernel magic (`0xAA55` boot sector plus
`bzImage` header `HdrS`) and print:

```
SD contains an x86 TinyCore/MicroCore image.
ESP32-P4 cannot run that ISO. Flash MicroCore-ESP32P4-*.img instead.
```

### ISO fallback

A hybrid ISO9660 image is a stretch goal so people who only ever Etcher
ISOs are happy. The loader's ISO9660 reader is read-only; persistent
`/tce` then needs a second FAT partition (Rock Ridge + extra FAT is
more work). **v1 ships MBR+FAT32 `.img` only.**

## PSRAM memory map (64 MB)

PSRAM window: `0x48000000` .. `0x4C000000` (64 MB). On 32 MB parts the
window ends at `0x4A000000`.

Suggested layout after the loader finishes (64 MB):

```
0x48000000  kernel Image (load + run in place)
            Linux uses remainder of DRAM for its own memblock
            initrd placed at the top of PSRAM, 2 MB aligned,
            and reserved via /chosen linux,initrd-start/end
0x4C000000  end (64 MB)
```

The loader will **not** add PSRAM to the ESP-IDF heap
(`CONFIG_SPIRAM_USE_MEMMAP=y`), matching why2025-linux: heap traffic
would overwrite the kernel.

Initrd at the top avoids colliding with the kernel's early BSS. The
DTB sits just below the initrd. Exact offsets are computed at runtime
from file sizes + detected PSRAM size.

Budget (64 MB, first-cut sizes):

| Region | Size |
|---|---|
| Kernel Image | ~4–8 MB |
| DTB | < 64 KB |
| `core.gz` (compressed, reserved until unpack) | ~4–8 MB |
| Unpacked initramfs + free RAM | remainder (~40+ MB) |

32 MB boards are supported only if `core.gz` stays small (~4 MB). The
image builder fails the 32 MB profile if the packed payloads leave
less than 12 MB free RAM.

## linux-loader (ESP-IDF)

Location: `bootloader/` (ESP-IDF v5.5.x, target `esp32p4`).

Responsibilities, in order:

1. **Bring-up** — UART banner, disable WDTs that would bite during SD
   reads, `esp_psram_init()`, print detected size.
2. **SDMMC** — 4-bit, onboard LDO if the board uses it, 1-bit fallback
   on CRC errors. No FAT VFS mount required: use `sdmmc_*` + a small
   FAT/ISO parser so we do not pull in a full IDF VFS stack.
3. **Config** — parse `loader.cfg`.
4. **Load** — stream files into PSRAM with progress on UART
   (`kernel 3.9/5.2 MB`).
5. **Handoff** — DTB patch, caches, jump.

Libraries:

- ESP-IDF `sdmmc`, `esp_psram`, `esp_partition` (only for our own app).
- Vendored or tiny built-in: MBR, FAT32 (8.3 + LFN), optional ISO9660.
- No FreeRTOS work after the jump; the jump is noreturn.

Board abstraction: `bootloader/boards/<name>.c` exports SDMMC slot
config, PSRAM LDO, and UART pins.

## Linux kernel

RV32 NOMMU, M-mode, based on a current LTS (6.18.x is the proven P4
port in why2025-linux). Work in `linux/` as a defconfig + patch series
applied by Buildroot, not a full kernel fork.

Minimum kernel features:

- `CONFIG_RISCV_M_MODE=y`, `CONFIG_MMU=n`
- UART earlycon / 8250 or ESP UART driver as available
- Initramfs support (`BLK_DEV_INITRD`)
- FAT/VFAT (to remount the SD after boot)
- MMC/`dw_mmc` (ESP32-P4 SDMMC)
- Squashfs (for `.tcz`)
- tmpfs, overlayfs or TinyCore's traditional copy-to-RAM for extensions
- `CONFIG_BINFMT_FLAT` (and/or static ELF if the NOMMU port allows it)

Device tree: `dts/esp32p4-microcore.dts` with:

- `memory@48000000` (size patched by the loader)
- UART0 as stdout
- SDMMC slot 0
- SYSTIMER as clocksource (as in the existing P4 Linux ports)

Wi-Fi/BT (ESP32-C6 companion on some boards) is **phase 2**, not
required to declare the bootloader done.

## MicroCore userspace

Buildroot LTS (2025.02.x) produces `core.gz`:

- BusyBox (ash, mount, vi, wget, tar, gzip)
- `/init` → TinyCore-like `tc-config` subset:
  - mount proc/sys/dev
  - find the FAT boot partition, mount at `/mnt/sd`
  - `tce-setup` from `/mnt/sd/tce`
  - restore `/home` and `/opt` from the card if present
  - `getty` on ttyS0, user `tc`, no password for v1
- `/tce` tooling: `tce-load`, `tce-ab`, even if the first repo is just
  a local directory of `.tcz` squashfs files
- No X11, no FLTK (that is TinyCore, not MicroCore)

`.tcz` format: squashfs of an RV32 root overlay. The first image can
ship zero extensions; the mechanism must work.

## Repository layout (to implement)

```
bootloader/                 ESP-IDF linux-loader
  main/
  boards/
  fat/                      MBR + FAT32 reader
  cfg/                      loader.cfg parser
linux/                      kernel.config + patches
dts/                        esp32p4-microcore.dts
rootfs/                     Buildroot overlay (MicroCore scripts)
configs/                    buildroot defconfig, idf sdkconfig.defaults
image/                      mkimg.sh → MicroCore-ESP32P4.img
tools/                      flash-p4.sh, serial helpers
docs/                       flashing guide (filled in as code lands)
PLAN.md                     this file
README.md                   user-facing summary
```

## Implementation phases

### Phase 0 — Repo skeleton and board config

- ESP-IDF project that inits PSRAM, prints size, inits SDMMC, lists
  the root directory of a FAT card over UART.
- Board Kconfig for Function-EV pins.
- Success: insert any FAT32 card, see filenames.

### Phase 1 — Load and jump

- FAT file read into PSRAM.
- Hand-built tiny `Image` (or why2025-style stub) that prints from
  UART after jump.
- Cache maintenance + PMP as required.
- Success: stub banner from PSRAM proves SD → PSRAM → jump.

### Phase 2 — Real kernel + core.gz from SD

- Buildroot RV32 NOMMU kernel + BusyBox initramfs named `vmlinuz` /
  `core.gz`.
- `loader.cfg` parsing, DTB patch, initrd reservation.
- Success: serial login from payloads on SD.

### Phase 3 — Rufus/Etcher image

- `image/mkimg.sh` builds a partitioned FAT32 `.img` with the layout
  above and a `README.txt`.
- Document Rufus: Image mode (DD), not ISO syslinux mode.
- Document BalenaEtcher: flash the `.img` directly.
- x86-ISO detection message.
- Success: a Windows-style flash of our image boots on hardware.

### Phase 4 — Extra storage and tce

- After boot, SD remounted rw at `/mnt/sd`.
- `/home` and `/opt` persist.
- `tce-load -i foo.tcz` from `/mnt/sd/tce`.
- Success: drop a file onto the card from a PC, see it in Linux;
  load one extension.

### Phase 5 — 64 MB profile and polish

- sdkconfig / DTB for 64 MB; auto-detect still works on 32 MB.
- Memory-budget check in `mkimg.sh`.
- Optional ISO9660 reader.
- Watchdog heartbeat on loader failure.

## Testing

No ESP32-P4 is assumed in CI. Split tests:

1. **Host unit tests** for MBR/FAT/cfg parsers (`bootloader/fat` built
   with a host gcc + fixture disk images).
2. **Image tests**: `mkimg.sh` output has the expected partitions and
   files (`mtools`/`sfdisk`/`file`).
3. **Hardware** (manual / later CI): PSRAM size banner, SD file list,
   kernel banner, login, persist a file on the card.

Until hardware is attached, Phase 0–3 must still compile and the
image/parser tests must pass.

## Non-goals (v1)

- Booting upstream x86 `Core-*.iso` or `TinyCore-*.iso`
- Replacing the ESP32 ROM / writing a from-scratch 1st-stage in Rust
- U-Boot (no mature ESP32-P4 SD boot path; IDF shim is the known-good)
- X11 / TinyCore GUI
- eMMC, USB mass-storage boot
- Secure boot / flash encryption (leave IDF defaults off)
- Wi-Fi via ESP32-C6

## Risks

| Risk | Mitigation |
|---|---|
| P4 Linux SDMMC driver is board-specific | Phase 0 proves IDF SDMMC first; kernel DTS copies a known-good P4 mmc node (Function EV / why2025). |
| 32 MB boards cannot hold a chubby core.gz | Keep BusyBox static and small; `mkimg` enforces free-RAM floor. |
| Jump-to-kernel cache/PMP bugs | Reuse why2025-linux handoff sequence; Phase 1 stub isolates this. |
| Rufus "ISO mode" tries to install syslinux | Ship `.img` and tell users to use DD/Image mode; Etcher has no ISO-hybrid trap if we do not ship an x86 ISO. |
| FAT LFN vs 8.3 | Support LFN; also keep all boot files 8.3-safe (`LOADER.CFG` alias). |

## References

- TinyCore boot model: kernel + `core.gz`, then `tc-config` / `tce-setup`
- why2025-linux: native RV32 NOMMU on ESP32-P4, PSRAM at `0x48000000`,
  IDF boot shim, SD usable *after* Linux starts (we move payload load
  *before* Linux)
- ESP-IDF P4 external RAM: up to 64 MB virtual window, ESP-PSRAM32/64
- ESP32-P4-Function-EV-Board SDMMC 4-bit GPIO 39–44, LDO 4
