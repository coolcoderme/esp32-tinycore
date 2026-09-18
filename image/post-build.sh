#!/bin/sh
# Make overlay scripts executable inside the target rootfs.
set -eu
TARGET=${1:-}
[ -n "$TARGET" ] || exit 0
chmod 0755 "$TARGET/init" 2>/dev/null || true
chmod 0755 "$TARGET/usr/bin/tce-load" "$TARGET/usr/bin/tce-setup" 2>/dev/null || true
