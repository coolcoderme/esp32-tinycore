# esp32-tinycore

Boot **MicroCore-style Linux** on an **ESP32-P4** from the onboard
microSD card. SPI flash holds only a small loader; the kernel,
`core.gz`, extensions, and extra storage live on the card.

Flash the SD image with **Rufus** or **BalenaEtcher**. Flash the loader
to the chip once with `esptool`.

> Official TinyCore/MicroCore ISOs from tinycorelinux.net are **x86**.
> They will not run on the P4 (RV32). This project ships a RISC-V
> MicroCore image with the same frugal layout (`/boot/vmlinuz`,
> `/boot/core.gz`, `/tce`).

## Quick start

```sh
make test          # host tests: FAT/MBR/GPT/cfg/DTB + a 256 MiB .img
make test-asan     # same under ASan/UBSan
make img           # MicroCore-ESP32P4.img for Rufus/Etcher
```

Loader (needs [ESP-IDF 5.5.x](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/get-started/)):

```sh
. $IDF_PATH/export.sh
make loader
tools/flash-p4.sh /dev/ttyUSB0
```

Then write `MicroCore-ESP32P4.img` to a microSD card (Rufus: **DD /
Image mode**), insert it, reset. Serial is **115200 8N1** on UART0.

Full steps: [docs/FLASHING.md](docs/FLASHING.md). GPIO / Wi-Fi / SSH:
[docs/PERIPHERALS.md](docs/PERIPHERALS.md). Architecture:
[PLAN.md](PLAN.md).

## What is in the tree

| Path | Role |
|---|---|
| `bootloader/` | ESP-IDF linux-loader (PSRAM, SDMMC, FAT/GPT, jump) |
| `bootloader/lib/` | Portable MBR/GPT/FAT/cfg/DTB/layout (host-tested) |
| `image/mkimg.py` | Builds the MBR+FAT32 `.img` (`image/mkimg.sh` wraps it) |
| `dts/` | Device trees (standalone + kernel) |
| `rootfs/overlay/` | `/init`, `tce-*`, `gpio`, `wifi-setup`, `ssh-setup` |
| `linux/` | Linux 6.18.35 P4 series (`gpio-esp32p4`, ESP-Hosted SDIO) + `kernel.config` |
| `tests/` | Host unit tests + image checks |
| `tools/` | `flash-p4.sh`, `console.sh`, `expand-fat.sh` |

`make img` packs a **placeholder** RISC-V Image until Buildroot has
produced `buildroot/output/images/Image`. The kernel series (gpio-esp32p4
+ ESP-Hosted SDIO) is in `linux/patches`. See
[linux/README.md](linux/README.md).

## Hardware

- ESP32-P4 with onboard SDMMC microSD (default: Function EV pins
  GPIO 39–44, LDO 4)
- ESP32-C6 on Function EV (SDIO 14–19, EN GPIO 54) for Wi-Fi
- 64 MB external PSRAM preferred (32 MB works if `core.gz` stays small)

## License

MIT for the loader and image tools. Kernel patches and DTS are
**GPL-2.0** (see `LICENSE`).
