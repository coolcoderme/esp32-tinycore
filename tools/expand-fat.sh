#!/bin/sh
# Expand partition 1 of a MicroCore disk (or .img) to fill remaining space.
#
# After Rufus/Etcher writes a 256 MiB image onto a larger card, Windows
# sees MICROCORE and unused space. This grows the FAT32 partition.
#
# Usage:
#   tools/expand-fat.sh /dev/sdX          # whole device (Linux)
#   tools/expand-fat.sh MicroCore-ESP32P4.img  # grow a sparse image first
#
# Growing the FAT filesystem itself needs `fatresize`. If it is missing,
# the partition table is still grown; format the extra space in Windows
# Disk Management, macOS Disk Utility, or `fatresize -s ...`.
set -eu
TARGET=${1:-}
if [ -z "$TARGET" ] || [ ! -e "$TARGET" ]; then
	echo "usage: $0 <device-or-image>" >&2
	exit 2
fi

# Grow partition 1 to the end of the device/image.
echo ", +" | sfdisk --no-reread -N 1 "$TARGET"

if command -v fatresize >/dev/null 2>&1; then
	# fatresize wants a partition device. On an image, loop-mount partition 1.
	if [ -b "$TARGET" ]; then
		part="${TARGET}1"
		[ -b "$part" ] || part="${TARGET}p1"
		fatresize -s max "$part"
	else
		echo "partition table grown. To grow FAT on this image:"
		echo "  fatresize -s max with a loop device, or copy to a card and expand there."
	fi
else
	echo "partition 1 now spans the rest of $TARGET."
	echo "Install fatresize, or expand the MICROCORE volume in Windows/macOS."
	echo "Boot files under /boot must be kept."
fi
