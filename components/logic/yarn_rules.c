#include "yarn_rules.h"
#include <math.h>

void yarn_basket(uint8_t i, int16_t *x, int16_t *y)
{
    static const int16_t B[YARN_BASKETS][2] = {{330, 318}, {136, 318}, {330, 150}, {136, 150}};
    *x = B[i % YARN_BASKETS][0];
    *y = B[i % YARN_BASKETS][1];
}

void yarn_chase_target(float yx, float yy, float cx, float cy, float *tx, float *ty)
{
    float dx = cx - yx, dy = cy - yy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 30.0f) {
        dx = 0.0f;
        dy = -1.0f;
        d = 1.0f;
    }
    *tx = yx + dx / d * YARN_CHASE_PX;
    *ty = yy + dy / d * YARN_CHASE_PX;
}

bool yarn_in_basket(float yx, float yy, int16_t bx, int16_t by)
{
    const float dx = yx - (float)bx, dy = yy - (float)(by - 10);
    return dx * dx + dy * dy < (float)(YARN_WIN_PX * YARN_WIN_PX);
}

float yarn_follow(float pos, float target, uint32_t dt_ms)
{
    return target + (pos - target) * expf(-(float)dt_ms / (float)YARN_FOLLOW_MS);
}
