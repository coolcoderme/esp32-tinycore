# Kernel patches

`dts/esp32p4-microcore.dts` is the board DTS. Drop it into an ESP32-P4
kernel tree (the why2025-linux series, or any tree with
`arch/riscv/boot/dts/espressif/` and the P4 CLIC/UART drivers) and add
it to that directory's Makefile:

```
dtb-$(CONFIG_SOC_ESP32P4) += esp32p4-microcore.dtb
```

`linux/microcore.config` is a fragment of extra options (initrd, VFAT,
dw_mmc, squashfs, loop, GPIO, cfg80211) to merge into that port's
defconfig.

## GPIO and Wi-Fi (not in mainline)

Vanilla Linux does not ship these P4 pieces. Take them from
[why2025-linux](https://github.com/mrbreaker/why2025-linux) (or an
equivalent port) and keep our DTS bindings:

| Feature | Binding / Kconfig | Upstream-ish source |
|---|---|---|
| GPIO banks 0/1 at `0x500E0000` | `espressif,esp32p4-gpio`, `CONFIG_GPIO_ESP32P4` | `patches/linux/0002-gpio-esp32p4.patch` |
| ESP32-C6 Wi-Fi | `espressif,esp-hosted-mcu` on SDIO slot 1, `wlan0` | `esp-hosted-ng` SPI/SDIO series |

The linux-loader already pulses **GPIO 54** (C6 EN) so the C6's
ESP-Hosted slave firmware is running before the kernel probes SDIO.

Function EV C6 SDIO pins: CLK=18, CMD=19, D0–D3=14–17.
