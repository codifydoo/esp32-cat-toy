#pragma once
#include <stdbool.h>
#include <stdint.h>

#define GESTURE_TAP_MAX_PX   20
#define GESTURE_SWIPE_MIN_PX 60
#define GESTURE_SWIPE_MAX_MS 400u
/* A swipe also needs |dx| >= 1.5 * |dy| (compared as 2|dx| >= 3|dy|). */

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
} gesture_kind_t;

typedef struct {
    bool active;
    int16_t x0, y0;
    uint32_t t0_ms;
    uint32_t max_dist2; /* farthest squared distance from the start point */
} gesture_track_t;

void gesture_begin(gesture_track_t *g, int16_t x, int16_t y, uint32_t t_ms);
void gesture_move(gesture_track_t *g, int16_t x, int16_t y);
gesture_kind_t gesture_end(gesture_track_t *g, int16_t x, int16_t y, uint32_t t_ms);
