#!/bin/sh
# Usage: tools/flash_and_log.sh <seconds> [extra idf.py args, e.g. -B build-diag ...]
set -e
SECS="${1:-15}"
[ $# -gt 0 ] && shift
PORT="${PORT:-$(ls /dev/cu.usbmodem* | head -1)}"
idf.py -p "$PORT" "$@" flash
# Let the flasher's own reset finish booting before the port opens (see serial_log.py).
sleep 3
python "$(dirname "$0")/serial_log.py" "$PORT" "$SECS"
