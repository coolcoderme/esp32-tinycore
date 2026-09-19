# GPIO, Wi-Fi, and SSH

These need a real RV32 kernel with the P4 GPIO driver and ESP-Hosted
(see [linux/README.md](../linux/README.md)). The SD image and `/init`
already look for config on the FAT volume.

## GPIO

```sh
gpio list
gpio out 5
gpio set 5 1
gpio get 5
```

Uses `/sys/class/gpio` or `gpioget`/`gpioset` (`libgpiod`) when present.
DTS nodes `gpio@500e0000` (bank 0, pins 0–31) and `gpio@500e1000`
(bank 1, pins 32–56) match [why2025-linux](https://github.com/mrbreaker/why2025-linux)
`gpio-esp32p4`.

**Do not toggle** pins the board already uses:

| Pins | Role |
|---|---|
| 14–19 | ESP32-C6 SDIO (Wi-Fi) |
| 35–36 | boot straps |
| 37–38 | UART0 console |
| 39–44 | microSD |
| 54 | C6 EN / CHIP_PU |

Prefer header pins **0–5** and **20–27**. Espressif also warns against
high-numbered GPIOs on some LDO rails.

## Wi-Fi (ESP32-C6)

The P4 has no radio. Function EV (and similar) boards put an ESP32-C6
on **SDIO slot 1**:

| Signal | P4 GPIO |
|---|---|
| CLK | 18 |
| CMD | 19 |
| D0–D3 | 14–17 |
| EN (reset) | 54 (active-high) |

The linux-loader pulses GPIO 54 before jumping so the C6's factory
ESP-Hosted slave firmware is running. Linux then needs an ESP-Hosted
host driver (`wlan0`).

Put this on the card as `opt/wifi.conf` (Windows can edit it):

```
ssid=YourNetwork
psk=YourPassphrase
```

`/init` runs `wifi-setup --auto`. Or at the prompt: `wifi-setup`.

C6 firmware is **not** the P4 flash image. Function EV boards ship
ESP-Hosted on the C6; reflash via the `PROG_C6` header if needed
([esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu)).

## SSH

Key-only. Dropbear does not accept the blank serial password.

1. On a PC, copy your public key to `home\tc\.ssh\authorized_keys`
   on the MICROCORE volume (or `home\tc\ssh\authorized_keys`).
2. Boot, wait for Wi-Fi.
3. `ssh tc@<wlan0-address>`

Host keys are generated on first boot and stored in `opt/dropbear/` so
`known_hosts` stays stable. FAT has no Unix permissions; `ssh-setup`
copies `authorized_keys` onto tmpfs before starting dropbear.

```sh
ssh-setup          # start now
ssh-setup --auto   # what /init runs
```
