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

size_t voice_fx_output_len(voice_t v, size_t in_len, uint32_t rate_hz);
/* Writes at most out_cap samples; returns how many were written. */
size_t voice_fx_apply(voice_t v, const int16_t *in, size_t in_len, int16_t *out, size_t out_cap, uint32_t rate_hz);
