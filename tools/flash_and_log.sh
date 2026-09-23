#!/bin/sh
# Usage: tools/flash_and_log.sh <seconds> [extra idf.py args, e.g. -B build-diag ...]
set -e
SECS="${1:-15}"
[ $# -gt 0 ] && shift
PORT="${PORT:-$(ls /dev/cu.usbmodem* | head -1)}"
idf.py -p "$PORT" "$@" flash
python "$(dirname "$0")/serial_log.py" "$PORT" "$SECS"
