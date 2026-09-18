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

See [linux/README.md](../linux/README.md). After Buildroot:

```sh
python3 image/mkimg.py \
  --kernel output/images/Image \
  --initrd output/images/rootfs.cpio.gz \
  --dtb    output/images/esp32p4-microcore.dtb \
  -o MicroCore-ESP32P4.img
```

Placeholder `vmlinuz` / `core.gz` in `make img` prove the SD layout and
the RISC-V Image magic the loader checks. They will not print a login
prompt until a P4 kernel is packed.

## Serial

```sh
tools/console.sh /dev/ttyUSB0
```
