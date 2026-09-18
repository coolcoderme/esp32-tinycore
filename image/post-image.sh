#!/bin/sh
# Pack Buildroot images into a Rufus/Etcher FAT32 disk image.
set -eu
IMAGES=${BINARIES_DIR:-${1:-}}
if [ -z "$IMAGES" ]; then
	echo "post-image: no BINARIES_DIR" >&2
	exit 1
fi
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
KERNEL=$IMAGES/Image
INITRD=$IMAGES/rootfs.cpio.gz
DTB=
for cand in "$IMAGES"/esp32p4-microcore.dtb "$IMAGES"/esp32p4.dtb; do
	if [ -f "$cand" ]; then
		DTB=$cand
		break
	fi
done
if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
	echo "post-image: missing Image or rootfs.cpio.gz in $IMAGES" >&2
	exit 0
fi
args="--kernel $KERNEL --initrd $INITRD -o $IMAGES/MicroCore-ESP32P4.img"
if [ -n "$DTB" ]; then
	args="$args --dtb $DTB"
fi
# shellcheck disable=SC2086
python3 "$ROOT/image/mkimg.py" $args
