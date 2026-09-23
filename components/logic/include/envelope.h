#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENVELOPE_MOUTH_B_PEAK 1500 /* quieter than this: mouth A (closed) */
#define ENVELOPE_MOUTH_C_PEAK 6000 /* louder than this: mouth C (wide open) */
#define REC_SPEECH_PEAK       1200 /* a chunk this loud counts as her voice */
#define REC_SILENCE_STOP_MS   1200

uint16_t envelope_peak(const int16_t *samples, size_t n);
uint8_t envelope_mouth(uint16_t peak);

typedef struct {
    bool heard;
    uint32_t quiet_samples;
} rec_stop_t;

void rec_stop_init(rec_stop_t *r);
/* Returns true when recording should stop: speech was heard and has been followed by 1.2 s of quiet. */
bool rec_stop_update(rec_stop_t *r, uint16_t chunk_peak, uint32_t chunk_samples, uint32_t rate_hz);
