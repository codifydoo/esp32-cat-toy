#include "touch_report.h"

uint8_t touch_report_parse(const uint8_t *d, size_t len, int16_t x[2], int16_t y[2])
{
    if (len < 7 || d[6] != TOUCH_REPORT_ACK) {
        return 0;
    }
    uint8_t reported = d[5] & 0x7F;
    uint8_t n = 0;
    for (uint8_t i = 0; i < reported && i < 2; i++) {
        size_t at = (size_t)i * 5 + (i ? 2 : 0);
        if (at + 4 >= len) {
            break;
        }
        const uint8_t *p = &d[at];
        if ((p[0] & 0x0F) != 0x06) {
            continue; /* not touching (e.g. lifting) */
        }
        x[n] = (int16_t)((p[1] << 4) | (p[3] >> 4));
        y[n] = (int16_t)((p[2] << 4) | (p[3] & 0x0F));
        n++;
    }
    return n;
}
