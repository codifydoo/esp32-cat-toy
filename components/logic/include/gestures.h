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

/* Parent entry (spec 3.1): two fingers held still for 5 s. */
#define GESTURE_HOLD_MS      5000u
#define GESTURE_HOLD_SHOW_MS 1000u /* the progress ring appears after this */
#define GESTURE_HOLD_MAX_PX  40    /* per point, from where it went down */

typedef enum {
    HOLD_IDLE = 0, /* fewer than two fingers, or this hold was cancelled or already fired */
    HOLD_WAITING,  /* two steady fingers, under 1 s: show nothing yet */
    HOLD_PROGRESS, /* ring visible: *progress runs 0..1000 over the remaining 4 s */
    HOLD_DONE,     /* returned once, at 5 s */
} hold_state_t;

typedef struct {
    bool active;  /* two fingers down and steady since t0_ms */
    bool spent;   /* fired or cancelled: stays idle until fewer than two fingers are down */
    int16_t x0[2], y0[2];
    uint32_t t0_ms;
} hold_track_t;

void hold_reset(hold_track_t *h);
/* Feed one touch sample with n points (only the first two are read). Finger order may change
 * between samples; each point is matched to the nearer start point. */
hold_state_t hold_update(hold_track_t *h, uint8_t n, const int16_t x[2], const int16_t y[2],
                         uint32_t t_ms, uint16_t *progress);
/* Whether the hold has taken over the touch: once the ring shows (1 s). Before that the touch
 * still belongs to the menu, so a quick two-finger tap or a palm still taps or wakes. */
bool hold_owns_touch(hold_state_t st);
