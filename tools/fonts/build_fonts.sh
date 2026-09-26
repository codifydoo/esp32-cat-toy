#!/bin/sh
# Converts Fredoka (assets-src/fonts, SIL OFL; see OFL.txt there) into the parent area's LVGL fonts
# (spec §3.13). The variable font gives its default instance, Regular. Output: components/ui/fonts/.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TTF="$ROOT/assets-src/fonts/Fredoka.ttf"
OUT="$ROOT/components/ui/fonts"
CONV="$ROOT/tools/fonts/node_modules/.bin/lv_font_conv"
[ -x "$CONV" ] || (cd "$ROOT/tools/fonts" && npm ci --silent --ignore-scripts)
# ASCII, Š Ž š ž and the "‹" of "‹ Parents" (Fredoka has no Č Ć Đ; the parent area's text is English).
TEXT="0x20-0x7E,0x160-0x161,0x17D-0x17E,0x2039"
# The big values (volume %, HH:MM, --:--) and the ± buttons.
DIGITS="0x20,0x25,0x2B,0x2D,0x30-0x3A"
mkdir -p "$OUT"
conv() { # name size range
    "$CONV" --font "$TTF" --size "$2" --bpp 4 --format lvgl --no-compress --lv-include lvgl.h \
        --range "$3" -o "$OUT/$1.c"
    echo "  $1"
}
conv font_fredoka_17 17 "$TEXT"
conv font_fredoka_20 20 "$TEXT"
conv font_fredoka_22 22 "$TEXT"
conv font_fredoka_46 46 "$DIGITS"
conv font_fredoka_54 54 "$DIGITS"
