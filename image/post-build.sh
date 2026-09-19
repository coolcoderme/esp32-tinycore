#!/bin/sh
# Make overlay scripts executable inside the target rootfs.
set -eu
TARGET=${1:-}
[ -n "$TARGET" ] || exit 0
chmod 0755 "$TARGET/init" 2>/dev/null || true
chmod 0755 "$TARGET/usr/bin/tce-load" "$TARGET/usr/bin/tce-setup" "$TARGET/usr/bin/tce-ab" \
	"$TARGET/usr/bin/gpio" "$TARGET/usr/bin/wifi-setup" "$TARGET/usr/bin/ssh-setup" 2>/dev/null || true
