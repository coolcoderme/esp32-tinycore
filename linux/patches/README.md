# Kernel patches

Applied by Buildroot via `BR2_LINUX_KERNEL_PATCH` against **Linux
6.18.35 LTS**. Filename sort is the apply order.

Patches `0001`–`0035` are the published
[why2025-linux](https://github.com/mrbreaker/why2025-linux) series
(GPL-2.0): RV32 NOMMU ESP32-P4 bring-up, **gpio-esp32p4**, dw_mmc
fixes, in-tree **ESP-Hosted-NG** (SPI sources plus unused SDIO
sources), CLIC/SYSTIMER/UART, M-mode userspace, MWDT.

Patches `0040`–`0042` are MicroCore additions:

| Patch | What it does |
|---|---|
| `0040-dts-esp32p4-microcore.patch` | Adds `esp32p4-microcore.dts` (GPIO banks + IRQ, MWDT, SDMMC slot 0 + slot 1, `espressif,esp_sdio`) |
| `0041-esp-hosted-sdio-host.patch` | Builds `sdio/esp_sdio.c` when `CONFIG_ESP_HOSTED_NG_SDIO=y` |
| `0042-mmc-dw_mmc-esp32p4-dual-slot.patch` | `snps,num-slots=<2>` so slot 1 enumerates the C6 |

`linux/kernel.config` is a complete 6.18.35 config (initrd, no
`CMDLINE_FORCE`, SDIO ESP-Hosted). `linux/microcore.config` is a
fragment merged on top.

The in-tree copy of `dts/esp32p4-microcore.dts` is the same board DTS
the 0040 patch drops into `arch/riscv/boot/dts/espressif/`.

## GPIO and Wi-Fi

| Feature | Binding / Kconfig |
|---|---|
| GPIO banks 0/1 at `0x500E0000` | `espressif,esp32p4-gpio`, `CONFIG_GPIO_ESP32P4` |
| ESP32-C6 Wi-Fi | `espressif,esp_sdio` on SDMMC slot 1, `CONFIG_ESP_HOSTED_NG=y` + `CONFIG_ESP_HOSTED_NG_SDIO=y` |

The linux-loader muxes C6 SDIO pins (14–19) and pulses **GPIO 54**
(C6 EN) so factory ESP-Hosted slave firmware is running before the
kernel probes slot 1.
