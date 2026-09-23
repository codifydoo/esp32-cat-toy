#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VOICE_MOUSE = 0, /* 1.5x faster: high and quick */
    VOICE_LION,      /* 0.67x speed: deep and slow */
    VOICE_ROBOT,     /* 30 Hz ring modulation + 8-bit crush + sample-and-hold x2 */
    VOICE_CAVE,      /* 250 ms echo, 4 repeats at half volume each */
    VOICE_COUNT
} voice_t;

/* Recordings play at the level of the built-in sounds (spec: assets are normalized to -3 dBFS peak). */
#define VOICE_FX_TARGET_PEAK 23197 /* -3 dBFS */
#define VOICE_FX_MAX_GAIN    16    /* +24 dB at most, so a quiet take doesn't turn into loud hiss */

size_t voice_fx_output_len(voice_t v, size_t in_len, uint32_t rate_hz);
/* Writes at most out_cap samples; returns how many were written. */
size_t voice_fx_apply(voice_t v, const int16_t *in, size_t in_len, int16_t *out, size_t out_cap, uint32_t rate_hz);
/* Scales buf so its peak reaches VOICE_FX_TARGET_PEAK, by at most VOICE_FX_MAX_GAIN. */
void voice_fx_normalize(int16_t *buf, size_t n);
