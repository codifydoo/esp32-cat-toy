#include "motion.h"
#include <math.h>

void shake_reset(shake_t *s) { *s = (shake_t){0}; }

bool shake_update(shake_t *s, float x, float y, float z, uint32_t t_ms)
{
    if (s->locked) {
        if (t_ms - s->locked_ms < SHAKE_LOCKOUT_MS) { /* unsigned: correct across the wrap */
            return false;
        }
        s->locked = false;
    }
    if (x * x + y * y + z * z <= SHAKE_G * SHAKE_G) {
        return false;
    }
    if (s->hits == SHAKE_SAMPLES) { /* keep the newest ones: drop the oldest */
        for (int i = 1; i < SHAKE_SAMPLES; i++) {
            s->hit_ms[i - 1] = s->hit_ms[i];
        }
        s->hits--;
    }
    s->hit_ms[s->hits++] = t_ms;
    if (s->hits == SHAKE_SAMPLES && t_ms - s->hit_ms[0] <= SHAKE_WINDOW_MS) {
        s->hits = 0;
        s->locked = true;
        s->locked_ms = t_ms;
        return true;
    }
    return false;
}

#define RAD_TO_DEG 57.2957795f

void tilt_reset(tilt_t *t)
{
    *t = (tilt_t){.level = {0.0f, 0.0f, 1.0f}};
}

void tilt_feed(tilt_t *t, float x, float y, float z)
{
    const float in[3] = {x, y, z};
    for (int i = 0; i < 3; i++) {
        t->up[i] = t->have ? t->up[i] + TILT_SMOOTHING * (in[i] - t->up[i]) : in[i];
    }
    t->have = true;
    if (t->recenter) {
        tilt_recenter(t);
    }
}

void tilt_recenter(tilt_t *t)
{
    if (!t->have) {
        t->recenter = true;
        return;
    }
    for (int i = 0; i < 3; i++) {
        t->level[i] = t->up[i];
    }
    t->recenter = false;
}

/* Turns u by the rotation that carries the level reading onto +z (Rodrigues' formula about the axis
 * level x z), so a toy held at its grip reads straight up. */
static void to_level_frame(const float level[3], const float u[3], float out[3])
{
    const float len = sqrtf(level[0] * level[0] + level[1] * level[1] + level[2] * level[2]);
    const float nx = len > 1e-6f ? level[0] / len : 0.0f;
    const float ny = len > 1e-6f ? level[1] / len : 0.0f;
    const float c = len > 1e-6f ? level[2] / len : 1.0f;
    const float s = sqrtf(nx * nx + ny * ny);
    if (s < 1e-4f) {
        /* the grip is flat (no turn needed) or face down (half a turn about x) */
        out[0] = u[0];
        out[1] = c > 0.0f ? u[1] : -u[1];
        out[2] = c > 0.0f ? u[2] : -u[2];
        return;
    }
    const float kx = ny / s, ky = -nx / s; /* unit axis (level x z) / s, in the screen plane */
    const float dot = kx * u[0] + ky * u[1];
    out[0] = u[0] * c + ky * u[2] * s + kx * dot * (1.0f - c);
    out[1] = u[1] * c - kx * u[2] * s + ky * dot * (1.0f - c);
    out[2] = u[2] * c + (kx * u[1] - ky * u[0]) * s;
}

bool tilt_angles(const tilt_t *t, float *x_deg, float *y_deg)
{
    if (!t->have) {
        return false;
    }
    float r[3];
    to_level_frame(t->level, t->up, r);
    *x_deg = atan2f(-r[0], r[2]) * RAD_TO_DEG;
    *y_deg = atan2f(-r[1], r[2]) * RAD_TO_DEG;
    return true;
}

bool tilt_is_level(float x_deg, float y_deg)
{
    return x_deg * x_deg + y_deg * y_deg <= TILT_DEAD_DEG * TILT_DEAD_DEG;
}

void tilt_speed(float x_deg, float y_deg, float *vx, float *vy)
{
    const float m = sqrtf(x_deg * x_deg + y_deg * y_deg);
    if (m <= TILT_DEAD_DEG) {
        *vx = 0.0f;
        *vy = 0.0f;
        return;
    }
    const float f = (m - TILT_DEAD_DEG) / (TILT_FULL_DEG - TILT_DEAD_DEG);
    const float speed = TILT_MAX_PX_S * (f > 1.0f ? 1.0f : f);
    *vx = speed * x_deg / m;
    *vy = speed * y_deg / m;
}

float roll_step(float *x, float *y, float x_deg, float y_deg, uint32_t dt_ms, float cx, float cy, float r)
{
    if (dt_ms > ROLL_MAX_DT_MS) {
        dt_ms = ROLL_MAX_DT_MS;
    }
    float vx, vy;
    tilt_speed(x_deg, y_deg, &vx, &vy);
    float nx = *x + vx * (float)dt_ms / 1000.0f;
    float ny = *y + vy * (float)dt_ms / 1000.0f;
    const float ex = nx - cx, ey = ny - cy, d = sqrtf(ex * ex + ey * ey);
    if (d > r) {
        nx = cx + ex / d * r;
        ny = cy + ey / d * r;
    }
    const float mx = nx - *x, my = ny - *y;
    *x = nx;
    *y = ny;
    return sqrtf(mx * mx + my * my);
}
