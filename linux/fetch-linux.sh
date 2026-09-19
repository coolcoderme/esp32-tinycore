#!/bin/sh
# Fetch Linux 6.18.35 LTS and apply the ESP32-P4 MicroCore patch series.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
VER=6.18.35
DEST=${LINUX_SRC:-"$ROOT/linux/src/linux-${VER}"}
TARBALL=${LINUX_TARBALL:-/tmp/linux-${VER}.tar.xz}
URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${VER}.tar.xz"
BUILD=0

for arg in "$@"; do
	case "$arg" in
	--build) BUILD=1 ;;
	--help|-h)
		echo "usage: $0 [--build]"
		echo "  downloads linux-${VER}, applies linux/patches/*.patch"
		echo "  --build  runs olddefconfig (needs CROSS_COMPILE / HOSTCC)"
		exit 0
		;;
	esac
done

mkdir -p "$(dirname "$DEST")" "$(dirname "$TARBALL")"
if [ ! -f "$TARBALL" ]; then
	echo "fetch $URL"
	curl -fsSL --retry 4 -o "$TARBALL" "$URL"
fi

if [ ! -f "$DEST/Makefile" ]; then
	echo "extract $TARBALL -> $DEST"
	mkdir -p "$DEST"
	tar -xJf "$TARBALL" --strip-components=1 -C "$DEST"
fi

echo "apply linux/patches"
for p in "$ROOT"/linux/patches/00*.patch; do
	echo "  $(basename "$p")"
	patch -p1 --forward --no-backup-if-mismatch -d "$DEST" < "$p"
done

cp "$ROOT/linux/kernel.config" "$DEST/.config"
if [ -f "$ROOT/linux/microcore.config" ]; then
	# fragment is documentation + extra y; full config already has them
	:
fi

echo "patched tree: $DEST"
if [ "$BUILD" -eq 1 ]; then
	if [ -z "${CROSS_COMPILE:-}" ]; then
		echo "CROSS_COMPILE is not set (e.g. riscv32-linux-)" >&2
		exit 1
	fi
	make -C "$DEST" ARCH=riscv olddefconfig
	echo "config ready; build with:"
	echo "  make -C $DEST ARCH=riscv CROSS_COMPILE=$CROSS_COMPILE -j\$(nproc) Image"
fi
