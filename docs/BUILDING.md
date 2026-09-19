# Build ESP32-P4 MicroCore

Phases from [PLAN.md](../PLAN.md). No board is required for 0–3 except
the final jump; host tests cover parsers and the disk image.

## Host (no ESP-IDF)

```sh
sudo apt-get install build-essential device-tree-compiler dosfstools mtools fdisk
make test          # FAT/MBR/GPT/cfg/DTB + 256 MiB .img
make test-asan     # same under ASan/UBSan
make img           # MicroCore-ESP32P4.img for Rufus/Etcher
```

`image/mkimg.sh` is a thin wrapper around `image/mkimg.py`.

## linux-loader (ESP-IDF 5.5.x)

```sh
. $IDF_PATH/export.sh
make loader
tools/flash-p4.sh /dev/ttyUSB0
```

Board overlays:

```sh
cd bootloader
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32_p4_psram64" set-target esp32p4 build
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.waveshare_esp32_p4" set-target esp32p4 build
```

## Kernel + core.gz

`linux/patches` is a real ESP32-P4 6.18.35 series (`gpio-esp32p4`,
ESP-Hosted-NG SDIO, dual-slot dw_mmc). See [linux/README.md](../linux/README.md).

```sh
git clone -b 2025.02.15 https://gitlab.com/buildroot.org/buildroot
patch -p1 -d buildroot -i configs/buildroot/patches/buildroot-tree/0001-package-wpa_supplicant-allow-nommu.patch
./linux/setup-paths.sh
cd buildroot
cp ../configs/buildroot/esp32p4_microcore_defconfig .config
make olddefconfig
make -j$(nproc)
```

Then from the repo root:

```sh
python3 image/mkimg.py \
  --kernel buildroot/output/images/Image \
  --initrd buildroot/output/images/rootfs.cpio.gz \
  --dtb    buildroot/output/images/esp32p4-microcore.dtb \
  -o MicroCore-ESP32P4.img
```

`make img` uses those files automatically when they exist. Without a
Buildroot build it still packs a placeholder Image so host tests can
check the FAT layout.

Kernel sources only (no userspace): `./linux/fetch-linux.sh`.

## GPIO / Wi-Fi / SSH

See [PERIPHERALS.md](PERIPHERALS.md). The kernel enables `gpio-esp32p4`
and ESP-Hosted-NG on SDIO slot 1. The loader pulses C6 EN (GPIO 54)
and muxes pins 14–19. Buildroot pulls in libgpiod, wpa_supplicant, iw,
and dropbear.

## Serial

```sh
tools/console.sh /dev/ttyUSB0
```
