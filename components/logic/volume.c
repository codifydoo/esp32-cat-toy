#include "volume.h"

uint8_t volume_step(uint8_t current, int dir)
{
    if (current < VOLUME_MIN) {
        return VOLUME_MIN;
    }
    if (current > VOLUME_MAX) {
        return VOLUME_MAX;
    }
    int v = current;
    if (dir > 0) {
        v = (v / VOLUME_STEP + 1) * VOLUME_STEP;
    } else if (dir < 0) {
        v = ((v + VOLUME_STEP - 1) / VOLUME_STEP - 1) * VOLUME_STEP;
    }
    return (uint8_t)(v < VOLUME_MIN ? VOLUME_MIN : v > VOLUME_MAX ? VOLUME_MAX : v);
}
