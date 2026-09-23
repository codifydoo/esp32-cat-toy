#!/bin/sh
# Regenerate everything in assets/ (images, sounds) and the C image manifest.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/test/run.sh" >/dev/null # also builds build-host/fxcli for the sound pipeline
(cd "$ROOT/tools/export_images" && npm ci --silent && node export.mjs)
"$ROOT/tools/build_sounds/build_sounds.sh"
du -sh "$ROOT/assets"
