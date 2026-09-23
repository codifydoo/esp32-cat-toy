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

void hold_reset(hold_track_t *h)
{
    *h = (hold_track_t){0};
}

static uint32_t dist2(int16_t ax, int16_t ay, int16_t bx, int16_t by)
{
    int32_t dx = ax - bx;
    int32_t dy = ay - by;
    return (uint32_t)(dx * dx + dy * dy);
}

static uint32_t max2(uint32_t a, uint32_t b) { return a > b ? a : b; }

hold_state_t hold_update(hold_track_t *h, uint8_t n, const int16_t x[2], const int16_t y[2],
                         uint32_t t_ms, uint16_t *progress)
{
    if (n < 2) {
        h->active = false;
        h->spent = false; /* a lifted finger ends this hold; the next two-finger touch starts fresh */
        return HOLD_IDLE;
    }
    if (h->spent) {
        return HOLD_IDLE;
    }
    if (!h->active) {
        h->active = true;
        for (int i = 0; i < 2; i++) {
            h->x0[i] = x[i];
            h->y0[i] = y[i];
        }
        h->t0_ms = t_ms;
        return HOLD_WAITING;
    }
    /* The controller may report the two fingers in either order, so use the pairing that moved less. */
    uint32_t same = max2(dist2(x[0], y[0], h->x0[0], h->y0[0]), dist2(x[1], y[1], h->x0[1], h->y0[1]));
    uint32_t swapped = max2(dist2(x[0], y[0], h->x0[1], h->y0[1]), dist2(x[1], y[1], h->x0[0], h->y0[0]));
    uint32_t moved = same < swapped ? same : swapped;
    if (moved > (uint32_t)(GESTURE_HOLD_MAX_PX * GESTURE_HOLD_MAX_PX)) {
        h->active = false;
        h->spent = true;
        return HOLD_IDLE;
    }
    uint32_t elapsed = t_ms - h->t0_ms; /* unsigned: correct across the counter wrapping */
    if (elapsed >= GESTURE_HOLD_MS) {
        h->active = false;
        h->spent = true;
        return HOLD_DONE;
    }
    if (elapsed < GESTURE_HOLD_SHOW_MS) {
        return HOLD_WAITING;
    }
    if (progress) {
        *progress = (uint16_t)((elapsed - GESTURE_HOLD_SHOW_MS) * 1000u / (GESTURE_HOLD_MS - GESTURE_HOLD_SHOW_MS));
    }
    return HOLD_PROGRESS;
}
