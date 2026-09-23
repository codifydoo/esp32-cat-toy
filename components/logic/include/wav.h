#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    size_t data_offset; /* byte offset of the first sample */
    size_t data_bytes;  /* clamped to the buffer and to whole 16-bit samples */
} wav_info_t;

/* Parses a RIFF/WAVE PCM file held in memory. Returns false for anything else. */
bool wav_parse(const uint8_t *buf, size_t len, wav_info_t *out);
