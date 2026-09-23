#include "voice_fx.h"
#include <math.h>

#define STEP_MOUSE_Q16 98304u /* 1.5 */
#define STEP_LION_Q16  43691u /* 2/3 */
#define ROBOT_RING_HZ  30.0f
#define CAVE_REPEATS   4u

static int16_t sat16(int32_t v)
{
    return (int16_t)(v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : v);
}

static size_t resample_len(size_t in_len, uint32_t step_q16)
{
    return in_len == 0 ? 0 : (size_t)((((uint64_t)(in_len - 1)) << 16) / step_q16) + 1;
}

static size_t resample(const int16_t *in, size_t in_len, int16_t *out, size_t out_cap, uint32_t step_q16)
{
    size_t total = resample_len(in_len, step_q16);
    size_t n = total < out_cap ? total : out_cap;
    for (size_t i = 0; i < n; i++) {
        uint64_t pos = (uint64_t)i * step_q16;
        size_t idx = (size_t)(pos >> 16);
        int64_t frac = (int64_t)(pos & 0xFFFFu);
        int32_t a = in[idx];
        int32_t b = idx + 1 < in_len ? in[idx + 1] : a;
        out[i] = (int16_t)(a + (int32_t)(((int64_t)(b - a) * frac) / 65536));
    }
    return n;
}

size_t voice_fx_output_len(voice_t v, size_t in_len, uint32_t rate_hz)
{
    switch (v) {
    case VOICE_MOUSE: return resample_len(in_len, STEP_MOUSE_Q16);
    case VOICE_LION: return resample_len(in_len, STEP_LION_Q16);
    case VOICE_CAVE: return in_len == 0 ? 0 : in_len + CAVE_REPEATS * (rate_hz / 4u);
    default: return in_len;
    }
}

size_t voice_fx_apply(voice_t v, const int16_t *in, size_t in_len, int16_t *out, size_t out_cap, uint32_t rate_hz)
{
    if (in_len == 0) {
        return 0;
    }
    switch (v) {
    case VOICE_MOUSE:
        return resample(in, in_len, out, out_cap, STEP_MOUSE_Q16);
    case VOICE_LION:
        return resample(in, in_len, out, out_cap, STEP_LION_Q16);
    case VOICE_ROBOT: {
        size_t n = in_len < out_cap ? in_len : out_cap;
        const float w = 2.0f * 3.14159265f * ROBOT_RING_HZ / (float)rate_hz;
        for (size_t i = 0; i < n; i += 2) {
            int32_t m = (int32_t)lrintf((float)in[i] * sinf(w * (float)i));
            int16_t q = (int16_t)((uint16_t)sat16(m) & 0xFF00u);
            out[i] = q;
            if (i + 1 < n) {
                out[i + 1] = q;
            }
        }
        return n;
    }
    case VOICE_CAVE: {
        size_t d = rate_hz / 4u;
        size_t total = voice_fx_output_len(v, in_len, rate_hz);
        size_t n = total < out_cap ? total : out_cap;
        for (size_t i = 0; i < n; i++) {
            int32_t acc = 0;
            for (size_t k = 0; k <= CAVE_REPEATS; k++) {
                size_t off = k * d;
                if (i >= off && i - off < in_len) {
                    acc += in[i - off] / (1 << k);
                }
            }
            out[i] = sat16(acc);
        }
        return n;
    }
    default:
        return 0;
    }
}

void voice_fx_normalize(int16_t *buf, size_t n)
{
    int32_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t a = buf[i] < 0 ? -(int32_t)buf[i] : buf[i];
        peak = a > peak ? a : peak;
    }
    if (peak == 0) {
        return;
    }
    int64_t gain_q16 = ((int64_t)VOICE_FX_TARGET_PEAK << 16) / peak;
    if (gain_q16 > ((int64_t)VOICE_FX_MAX_GAIN << 16)) {
        gain_q16 = (int64_t)VOICE_FX_MAX_GAIN << 16;
    }
    for (size_t i = 0; i < n; i++) {
        buf[i] = sat16((int32_t)(((int64_t)buf[i] * gain_q16) / 65536));
    }
}
