#!/bin/sh
# Flash linux-loader to an ESP32-P4. Usage: tools/flash-p4.sh /dev/ttyUSB0
set -eu
PORT=${1:-/dev/ttyUSB0}
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BL=$ROOT/bootloader/build

if [ ! -f "$BL/linux-loader.bin" ]; then
	echo "build the loader first:" >&2
	echo "  . \$IDF_PATH/export.sh && cd bootloader && idf.py set-target esp32p4 build" >&2
	exit 1
fi

esptool --chip esp32p4 -p "$PORT" -b 460800 \
	--before default-reset --after hard-reset \
	write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
	0x2000  "$BL/bootloader/bootloader.bin" \
	0x8000  "$BL/partition_table/partition-table.bin" \
	0x10000 "$BL/linux-loader.bin"
