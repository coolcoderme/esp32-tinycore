# Flashing ESP32-P4 MicroCore

Two pieces of media:

1. **SPI flash** on the ESP32-P4 — the linux-loader (once).
2. **microSD card** — MicroCore (`vmlinuz` + `core.gz` + extra storage).

## 1. Loader (SPI flash)

Needs [ESP-IDF v5.5.x](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/get-started/)
and `esptool`.

```sh
. $IDF_PATH/export.sh
cd bootloader
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 -b 460800 flash
```

Or with esptool directly:

```sh
esptool --chip esp32p4 -p /dev/ttyUSB0 -b 460800 \
  --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x2000  bootloader/build/bootloader/bootloader.bin \
  0x8000  bootloader/build/partition_table/partition-table.bin \
  0x10000 bootloader/build/linux-loader.bin
```

`tools/flash-p4.sh` wraps that.

Board profiles:

```sh
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32_p4_psram64" build
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.waveshare_esp32_p4" build
```

Default pins are the Espressif Function EV Board SD slot (GPIO 39–44,
LDO 4). 64 MB vs 32 MB PSRAM is probed at runtime.

## 2. SD card (Rufus / BalenaEtcher)

Build an image:

```sh
python3 image/mkimg.py -o MicroCore-ESP32P4.img
```

That packs placeholder `vmlinuz` / `core.gz` if you have not built Linux
yet. After Buildroot:

```sh
python3 image/mkimg.py \
  --kernel path/to/Image \
  --initrd path/to/rootfs.cpio.gz \
  --dtb    path/to/esp32p4-microcore.dtb \
  -o MicroCore-ESP32P4.img
```

### Rufus (Windows)

1. Device: the SD card (double-check the drive letter).
2. Boot selection: **Disk or ISO image** → select `MicroCore-ESP32P4.img`.
3. If Rufus offers ISO vs DD: choose **DD Image mode**. Do not let it
   install syslinux; the ESP32 ROM cannot run that.
4. Start.

### BalenaEtcher

Select `MicroCore-ESP32P4.img`, select the SD card, Flash. No extra
options.

### After flashing

Windows will see a drive named **MICROCORE**. You can drop files in
`home\` (persistent extra storage) or `tce\` (`.tcz` extensions listed
in `tce\onboot.lst`). Do not delete `boot\`.

If the card is larger than the 256 MiB image, leftover space is
unpartitioned. Expand with Windows Disk Management or:

```sh
tools/expand-fat.sh /dev/sdX
```

## 3. Boot

Insert the card, reset the P4. Serial console:

```
115200 8N1  UART0
```

```sh
tio -b 115200 /dev/ttyUSB0
# or
tools/console.sh /dev/ttyUSB0
```

You should see `=== ESP32-P4 linux-loader (SD MicroCore) ===`, PSRAM
size, a FAT directory listing, then kernel load progress.

## What will not work

Official `Core-*.iso` / `TinyCore-*.iso` from tinycorelinux.net are
**x86**. The loader looks for the `HdrS` bzImage header and prints:

```
SD contains an x86 TinyCore/MicroCore image.
ESP32-P4 cannot run that ISO. Flash MicroCore-ESP32P4-*.img instead.
```
