#!/bin/sh
# Serial console helper. Usage: tools/console.sh [/dev/ttyUSB0]
set -eu
PORT=${1:-/dev/ttyUSB0}
BAUD=${2:-115200}

if command -v tio >/dev/null 2>&1; then
	exec tio -b "$BAUD" "$PORT"
fi
if command -v minicom >/dev/null 2>&1; then
	exec minicom -D "$PORT" -b "$BAUD"
fi
if command -v screen >/dev/null 2>&1; then
	exec screen "$PORT" "$BAUD"
fi
exec python3 - "$PORT" "$BAUD" <<'PY'
import sys, serial
port, baud = sys.argv[1], int(sys.argv[2])
ser = serial.Serial(port, baud, timeout=0.2)
print(f"connected {port} {baud}; Ctrl-C to exit", file=sys.stderr)
try:
    while True:
        data = ser.read(256)
        if data:
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
except KeyboardInterrupt:
    pass
PY
