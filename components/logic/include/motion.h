#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Shake (spec §7): acceleration above 2.2 g for 3 samples within 300 ms, then a 1.5 s lockout. */
#define SHAKE_G          2.2f
#define SHAKE_SAMPLES    3
#define SHAKE_WINDOW_MS  300u
#define SHAKE_LOCKOUT_MS 1500u

typedef struct {
    uint32_t hit_ms[SHAKE_SAMPLES]; /* times of the latest samples above SHAKE_G, oldest first */
    uint8_t hits;
    bool locked;
    uint32_t locked_ms;
} shake_t;

void shake_reset(shake_t *s);
/* Feed one accelerometer sample in g (any axes). True once per shake. */
bool shake_update(shake_t *s, float x, float y, float z, uint32_t t_ms);

/* Tilt (spec §7 and §3.10). Axes are the screen's: x to the right, y toward the bottom (USB) edge, z
 * out of the glass. Feed the accelerometer reading, which is +1 g "up": z = +1 lying flat face up.
 * Level is the grip at tilt_recenter() (flat until then), and the angles say how far "down" has
 * moved from it. */
#define TILT_DEAD_DEG  8.0f
#define TILT_FULL_DEG  25.0f
#define TILT_MAX_PX_S  240.0f
#define TILT_SMOOTHING 0.25f /* weight of each new 50 Hz sample: one bump alone stays under 8° */
#define ROLL_MAX_DT_MS 100u  /* a longer gap (a stall) rolls as if 100 ms passed */

typedef struct {
    float up[3];    /* low-passed reading */
    float level[3]; /* the reading that counts as level */
    bool have;      /* at least one sample fed */
    bool recenter;  /* the next sample becomes level */
} tilt_t;

void tilt_reset(tilt_t *t); /* level = flat, no samples yet */
void tilt_feed(tilt_t *t, float x, float y, float z);
void tilt_recenter(tilt_t *t); /* the current grip becomes level (the next sample, if none yet) */
/* x_deg > 0: the right edge is lower than at the grip, so a ball rolls right; y_deg > 0: the bottom
 * edge is lower, so it rolls toward the bottom. False before the first sample. */
bool tilt_angles(const tilt_t *t, float *x_deg, float *y_deg);
bool tilt_is_level(float x_deg, float y_deg); /* inside the round 8° dead zone */
void tilt_speed(float x_deg, float y_deg, float *vx, float *vy); /* px/s along the screen axes */
/* Rolls a ball centered at (*x, *y) for dt_ms at the tilt's speed, keeping its center within r of
 * (cx, cy). Returns how far it moved, in px (0 when level or pinned against the rim). */
float roll_step(float *x, float *y, float x_deg, float y_deg, uint32_t dt_ms, float cx, float cy, float r);
