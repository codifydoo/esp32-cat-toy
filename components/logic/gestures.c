#include "gestures.h"
#include <stdlib.h>

void gesture_begin(gesture_track_t *g, int16_t x, int16_t y, uint32_t t_ms)
{
    g->active = true;
    g->x0 = x;
    g->y0 = y;
    g->t0_ms = t_ms;
    g->max_dist2 = 0;
}

static void track(gesture_track_t *g, int16_t x, int16_t y)
{
    int32_t dx = x - g->x0;
    int32_t dy = y - g->y0;
    uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
    if (d2 > g->max_dist2) {
        g->max_dist2 = d2;
    }
}

void gesture_move(gesture_track_t *g, int16_t x, int16_t y)
{
    if (g->active) {
        track(g, x, y);
    }
}

gesture_kind_t gesture_end(gesture_track_t *g, int16_t x, int16_t y, uint32_t t_ms)
{
    if (!g->active) {
        return GESTURE_NONE;
    }
    g->active = false;
    track(g, x, y);
    if (g->max_dist2 <= (uint32_t)(GESTURE_TAP_MAX_PX * GESTURE_TAP_MAX_PX)) {
        return GESTURE_TAP;
    }
    int32_t dx = x - g->x0;
    int32_t dy = y - g->y0;
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);
    if (adx >= GESTURE_SWIPE_MIN_PX && 2 * adx >= 3 * ady && t_ms - g->t0_ms <= GESTURE_SWIPE_MAX_MS) {
        return dx < 0 ? GESTURE_SWIPE_LEFT : GESTURE_SWIPE_RIGHT;
    }
    return GESTURE_NONE;
}
