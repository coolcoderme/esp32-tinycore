#!/bin/sh
# Wrapper so `image/mkimg.sh` matches PLAN.md. Forwards to mkimg.py.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
exec python3 "$ROOT/image/mkimg.py" "$@"
