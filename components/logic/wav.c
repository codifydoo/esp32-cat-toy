#include "wav.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool wav_parse(const uint8_t *buf, size_t len, wav_info_t *out)
{
    if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        return false;
    }
    bool have_fmt = false;
    size_t p = 12;
    while (p + 8 <= len) {
        uint32_t size = rd32(buf + p + 4);
        const uint8_t *body = buf + p + 8;
        size_t avail = len - (p + 8);
        if (memcmp(buf + p, "fmt ", 4) == 0) {
            if (size < 16 || avail < 16 || rd16(body) != 1) {
                return false;
            }
            out->channels = rd16(body + 2);
            out->rate = rd32(body + 4);
            out->bits = rd16(body + 14);
            have_fmt = true;
        } else if (memcmp(buf + p, "data", 4) == 0) {
            if (!have_fmt) {
                return false;
            }
            out->data_offset = p + 8;
            out->data_bytes = (size < avail ? size : avail) & ~(size_t)1;
            return true;
        }
        size_t step = 8 + (size_t)size + (size & 1u);
        if (step > len - p) {
            break;
        }
        p += step;
    }
    return false;
}
