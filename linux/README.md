# Linux for ESP32-P4 MicroCore

The bootloader in `bootloader/` will jump to any RV32 Image it finds as
`/boot/vmlinuz` plus `/boot/core.gz`. Producing those files needs a
kernel that actually runs on the P4.

## Kernel port

Vanilla mainline Linux does not yet bring up ESP32-P4 UART / CLIC /
SDMMC on its own. The working reference is the RV32 NOMMU M-mode port
in [why2025-linux](https://github.com/mrbreaker/why2025-linux) (Linux
6.18 LTS + an ESP-IDF boot shim). Use that patch series (or an
equivalent P4 kernel) as the base, then add our DTS:

- `dts/esp32p4-microcore.dts` — UART0, SYSTIMER, SDMMC slot 0, GPIO
  banks, C6 ESP-Hosted child node, 64 MB memory placeholders
- `dts/esp32p4-microcore-standalone.dts` — same placeholders, no
  kernel includes; this is what `image/mkimg.py` compiles for the SD
  card until Buildroot produces a real `esp32p4.dtb`

Copy the full DTS into `arch/riscv/boot/dts/espressif/` of the patched
kernel and add `dtb-$(CONFIG_SOC_ESP32P4) += esp32p4-microcore.dtb` to
that directory's Makefile.

## Required config (on top of the P4 port)

See `linux/microcore.config`. In particular:

- `CONFIG_BLK_DEV_INITRD=y` and `CONFIG_RD_GZIP=y` (`core.gz`)
- VFAT, `dw_mmc`, squashfs, tmpfs, loop (for `.tcz`)
- `CONFIG_DEVTMPFS=y`
- `CONFIG_GPIO_ESP32P4=y` (why2025 `gpio-esp32p4` patch) plus
  `CONFIG_GPIO_SYSFS` / `CONFIG_GPIO_CDEV`
- `CONFIG_NET` / `CONFIG_INET` / `CONFIG_CFG80211` and the P4 port's
  ESP-Hosted host driver so `wlan0` appears
- FLAT or static uClibc/musl NOMMU userland

Wi-Fi and SSH add size. Prefer 64 MB PSRAM; keep `core.gz` small on
32 MB boards (`mkimg` still enforces a 12 MiB free-RAM floor).

## Buildroot

`configs/buildroot/esp32p4_microcore_defconfig` wires a Buildroot 2025.02
LTS tree to this overlay:

```
git clone -b 2025.02.15 https://gitlab.com/buildroot.org/buildroot
cd buildroot
make BR2_EXTERNAL=/path/to/esp32-tinycore/configs/buildroot \
     defconfig  # or copy the defconfig in
```

Until the P4 kernel tree is pointed at `BR2_LINUX_KERNEL_*`, Buildroot
will not emit a bootable `Image`. You can still pack a placeholder with
`python3 image/mkimg.py`.

After a real kernel build, pack the SD image:

```
python3 image/mkimg.py \
    --kernel buildroot/output/images/Image \
    --initrd buildroot/output/images/rootfs.cpio.gz \
    --dtb    buildroot/output/images/esp32p4-microcore.dtb \
    -o MicroCore-ESP32P4.img
```

Rename / copy so the card sees `vmlinuz` and `core.gz` — `mkimg.py`
does that rename when copying into the FAT volume.
