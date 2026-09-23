#!/usr/bin/env bash
# Build every sound in sounds.txt into assets/snd/<name>.wav (24 kHz, 16-bit, mono).
# Real recordings in assets-src/sounds/{phrases,sfx}/<name>.wav win over generated placeholders.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/assets-src/sounds"
OUT="$ROOT/assets/snd"
TMP="$ROOT/build-assets/snd"
FXCLI="$ROOT/build-host/fxcli"
RATE=24000
PHRASE_PITCH_CENTS=400 # spec 6.2: phrases pitched up 4 semitones into the cat voice
GAP_S=0.08

[ -x "$FXCLI" ] || { echo "error: $FXCLI missing. Run test/run.sh first." >&2; exit 1; }
command -v sox >/dev/null || { echo "error: sox missing (brew install sox)" >&2; exit 1; }

rm -rf "$OUT" "$TMP"
mkdir -p "$OUT" "$TMP"
sox -n -r "$RATE" -c 1 -b 16 "$TMP/gap.wav" trim 0 "$GAP_S"

# finish <in> <out> [sox effects applied before trimming and normalizing]
finish() {
    local in="$1" out="$2"
    shift 2
    sox "$in" -r "$RATE" -c 1 -b 16 -e signed-integer "$out" "$@" \
        silence 1 0.02 0.5% reverse silence 1 0.02 0.5% reverse norm -3
}

placeholders=""

while read -r name kind spec; do
    case "$name" in '' | '#'*) continue ;; esac
    out="$OUT/$name.wav"
    case "$kind" in
    phrase)
        if [ -f "$SRC/phrases/$name.wav" ]; then
            src="$SRC/phrases/$name.wav"
        else
            src="$TMP/$name.aiff"
            say -v Lana -o "$src" "$spec"
            placeholders="$placeholders $name"
        fi
        finish "$src" "$out" pitch "$PHRASE_PITCH_CENTS"
        ;;
    synth)
        python3 "$HERE/synth.py" "$spec" "$TMP/$name.wav"
        finish "$TMP/$name.wav" "$out"
        ;;
    organic)
        if [ -f "$SRC/sfx/$name.wav" ]; then
            finish "$SRC/sfx/$name.wav" "$out"
        else
            python3 "$HERE/synth.py" "$spec" "$TMP/$name.wav"
            finish "$TMP/$name.wav" "$out"
            placeholders="$placeholders $name"
        fi
        ;;
    pitch)
        base="${spec%%:*}"
        semis="${spec##*:}"
        sox "$OUT/$base.wav" "$out" pitch $((semis * 100))
        ;;
    concat)
        parts=()
        i=0
        IFS='+' read -ra items <<<"$spec"
        for item in "${items[@]}"; do
            src="$OUT/${item%%@*}.wav"
            if [[ "$item" == *@* ]]; then
                sox "$src" -t raw -r "$RATE" -e signed-integer -b 16 -c 1 "$TMP/$name.$i.raw"
                "$FXCLI" "${item##*@}" "$RATE" "$TMP/$name.$i.raw" "$TMP/$name.$i.fx.raw"
                sox -t raw -r "$RATE" -e signed-integer -b 16 -c 1 "$TMP/$name.$i.fx.raw" "$TMP/$name.$i.wav"
                src="$TMP/$name.$i.wav"
            fi
            if [ "$i" -gt 0 ]; then
                parts+=("$TMP/gap.wav")
            fi
            parts+=("$src")
            i=$((i + 1))
        done
        sox "${parts[@]}" "$out" norm -3
        ;;
    *)
        echo "error: unknown kind '$kind' for $name" >&2
        exit 1
        ;;
    esac
    echo "  $name"
done <"$HERE/sounds.txt"

echo "Built $(ls "$OUT" | wc -l | tr -d ' ') sounds into assets/snd"
if [ -n "$placeholders" ]; then
    echo "Placeholders (replace them via docs/recording-list.md):$placeholders"
fi
