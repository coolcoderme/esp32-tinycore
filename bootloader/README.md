# Bootloader for ESP32-P4 MicroCore Linux on SD

A small ESP-IDF app that lives in SPI flash, initialises 32/64 MB
PSRAM, pulses the ESP32-C6 EN line (GPIO 54 on Function EV) so
ESP-Hosted Wi-Fi can come up later, reads a TinyCore-style FAT32 card
from the onboard SD slot, and jumps to an RV32 kernel Image.

See [PLAN.md](../PLAN.md) and [docs/FLASHING.md](../docs/FLASHING.md).

```
cd bootloader
. $IDF_PATH/export.sh
idf.py set-target esp32p4
idf.py build
```
