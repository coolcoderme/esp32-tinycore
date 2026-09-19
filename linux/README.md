# Linux for ESP32-P4 MicroCore

This directory is a **real** RV32 NOMMU ESP32-P4 kernel: Linux 6.18.35
LTS plus the why2025-linux port (`gpio-esp32p4`, CLIC, SYSTIMER, UART,
dw_mmc) and ESP-Hosted-NG (SDIO transport for Function EV).

Vanilla mainline does not bring up P4 UART / CLIC / SDMMC / GPIO.

## What you get

- `linux/patches/` — apply-ordered series (`0001`–`0035` why2025,
  `0040`–`0042` MicroCore DTS + SDIO + dual-slot MMC)
- `linux/kernel.config` — complete defconfig (initrd, VFAT, dw_mmc,
  `CONFIG_GPIO_ESP32P4=y`, `CONFIG_ESP_HOSTED_NG_SDIO=y`)
- `linux/microcore.config` — fragment merged on top
- `dts/esp32p4-microcore.dts` — board DTS compiled into
  `esp32p4-microcore.dtb`

After Buildroot, `output/images/` has `Image`, `rootfs.cpio.gz`, and
(if DTS support is on) `esp32p4-microcore.dtb`. `image/post-image.sh`
packs those into `MicroCore-ESP32P4.img`.

## Buildroot

```sh
git clone -b 2025.02.15 https://gitlab.com/buildroot.org/buildroot
# Allow wpa_supplicant on NOMMU (upstream gates it on fork())
patch -p1 -d buildroot -i configs/buildroot/patches/buildroot-tree/0001-package-wpa_supplicant-allow-nommu.patch

./linux/setup-paths.sh
cd buildroot
cp ../configs/buildroot/esp32p4_microcore_defconfig .config
make olddefconfig
make -j$(nproc)
```

`setup-paths.sh` rewrites `@ESP32_TINYCORE@` in the defconfig to this
repo's absolute path (Buildroot wants absolute `BR2_LINUX_KERNEL_PATCH`
/ config paths).

Or point Buildroot at the series by hand:

```sh
make BR2_LINUX_KERNEL_PATCH=$PWD/../linux/patches \
     BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE=$PWD/../linux/kernel.config \
     olddefconfig
```

Kernel-only fetch/apply (no Buildroot):

```sh
./linux/fetch-linux.sh          # downloads 6.18.35 and applies patches
./linux/fetch-linux.sh --build  # also olddefconfig (needs a riscv32 toolchain)
```

Pack the SD image from Buildroot output:

```sh
python3 image/mkimg.py \
    --kernel buildroot/output/images/Image \
    --initrd buildroot/output/images/rootfs.cpio.gz \
    --dtb    buildroot/output/images/esp32p4-microcore.dtb \
    -o MicroCore-ESP32P4.img
```

`make img` uses those files automatically when they exist; otherwise it
still packs a placeholder Image so host tests can check the FAT layout.

## Hardware notes

- **gpio-esp32p4** exports `/dev/gpiochip0` (0–31) and
  `/dev/gpiochip1` (32–56).
- **ESP-Hosted** on Function EV is SDIO slot 1. The C6 must run
  ESP-Hosted slave firmware (factory image on Function EV). `wlan0`
  appears when the SDIO function enumerates.
- Bootargs are **not** forced: the linux-loader writes `loader.cfg`
  `append` into `/chosen/bootargs`. Keep `ipv6.disable=1` (why2025:
  IPv6 `rs_timer` hung `wlan0 up`).
