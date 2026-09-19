#!/bin/sh
# Substitute @ESP32_TINYCORE@ with this repo's absolute path.
# Buildroot .config and BR2_LINUX_KERNEL_* only accept absolute paths.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
for f in \
	"$ROOT/configs/buildroot/esp32p4_microcore_defconfig"
do
	[ -f "$f" ] || continue
	sed -i.bak "s|@ESP32_TINYCORE@|$ROOT|g" "$f"
	rm -f "$f.bak"
done
if grep -R -n '@ESP32_TINYCORE@' "$ROOT/configs" "$ROOT/linux" >/dev/null 2>&1; then
	echo "setup-paths: leftover @ESP32_TINYCORE@ tokens:" >&2
	grep -R -n '@ESP32_TINYCORE@' "$ROOT/configs" "$ROOT/linux" >&2 || true
	exit 1
fi
echo "setup-paths: $ROOT"
