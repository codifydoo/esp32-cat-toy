#include "envelope.h"

uint16_t envelope_peak(const int16_t *samples, size_t n)
{
    uint16_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t v = samples[i];
        uint16_t a = (uint16_t)(v < 0 ? (v == INT16_MIN ? INT16_MAX : -v) : v);
        if (a > peak) {
            peak = a;
        }
    }
    return peak;
}

uint8_t envelope_mouth(uint16_t peak)
{
    if (peak < ENVELOPE_MOUTH_B_PEAK) {
        return 0;
    }
    return peak < ENVELOPE_MOUTH_C_PEAK ? 1 : 2;
}

void rec_stop_init(rec_stop_t *r)
{
    r->heard = false;
    r->quiet_samples = 0;
}

bool rec_stop_update(rec_stop_t *r, uint16_t chunk_peak, uint32_t chunk_samples, uint32_t rate_hz)
{
    if (chunk_peak >= REC_SPEECH_PEAK) {
        r->heard = true;
        r->quiet_samples = 0;
        return false;
    }
    r->quiet_samples += chunk_samples;
    return r->heard && (uint64_t)r->quiet_samples * 1000u >= (uint64_t)REC_SILENCE_STOP_MS * rate_hz;
}
