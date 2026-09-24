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
