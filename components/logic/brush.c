#include "brush.h"
#include <math.h>

#define DEG_PER_RAD 57.2957795f

void brush_init(brush_t *b, brush_kind_t kind, int16_t cx, int16_t cy)
{
    *b = (brush_t){.kind = kind, .cx = cx, .cy = cy};
}

void brush_clear(brush_t *b)
{
    b->dots = 0;
    b->paws = 0;
    b->down = false;
}

void brush_up(brush_t *b) { b->down = false; }

static bool inside(const brush_t *b, float x, float y)
{
    const float dx = x - b->cx, dy = y - b->cy;
    return dx * dx + dy * dy <= (float)(BRUSH_AREA_R * BRUSH_AREA_R);
}

/* One stamp for the stroke point (x, y); ux, uy is the stroke direction (0, 0 at the first touch). */
static size_t stamp(brush_t *b, float x, float y, float ux, float uy, stamp_t *out)
{
    if (!inside(b, x, y)) {
        return 0;
    }
    if (b->kind == BRUSH_RAINBOW) {
        *out = (stamp_t){(int16_t)lroundf(x), (int16_t)lroundf(y),
                         (uint8_t)((b->dots / BRUSH_COLOR_EVERY) % BRUSH_COLORS), 0};
        b->dots++;
        return 1;
    }
    /* Design: the angle follows the stroke (0 at the first print), and prints alternate sides. */
    const float angle = (ux == 0.0f && uy == 0.0f) ? 0.0f : atan2f(uy, ux) * DEG_PER_RAD + 90.0f;
    const float side = (b->paws % 2) ? 1.0f : -1.0f;
    const float rad = angle / DEG_PER_RAD;
    *out = (stamp_t){(int16_t)lroundf(x + cosf(rad) * BRUSH_PAW_SIDE_PX * side),
                     (int16_t)lroundf(y + sinf(rad) * BRUSH_PAW_SIDE_PX * side), (uint8_t)(b->paws % BRUSH_COLORS),
                     (int16_t)lroundf(angle)};
    b->paws++;
    return 1;
}

size_t brush_down(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS])
{
    b->down = true;
    b->lx = x;
    b->ly = y;
    return stamp(b, x, y, 0.0f, 0.0f, out);
}

size_t brush_move(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS])
{
    if (!b->down) {
        return 0;
    }
    const float step = b->kind == BRUSH_RAINBOW ? BRUSH_DOT_STEP_PX : BRUSH_PAW_STEP_PX;
    float dx = (float)x - b->lx, dy = (float)y - b->ly;
    float d = sqrtf(dx * dx + dy * dy);
    size_t n = 0;
    int steps = 0;
    while (d >= step) {
        if (steps == BRUSH_MAX_STAMPS) {
            b->lx = x; /* a jump too long to fill: carry on from where the finger is */
            b->ly = y;
            return n;
        }
        const float ux = dx / d, uy = dy / d;
        b->lx += ux * step;
        b->ly += uy * step;
        steps++;
        n += stamp(b, b->lx, b->ly, ux, uy, &out[n]);
        dx = (float)x - b->lx;
        dy = (float)y - b->ly;
        d = sqrtf(dx * dx + dy * dy);
    }
    return n;
}

void paw_blobs(const stamp_t *s, blob_t out[5])
{
    /* The design's 34 px paw (a 24-unit path scaled by 34/24), relative to its center, toes up. */
    static const blob_t PAW[5] = {
        {0.0f, 4.96f, 7.0f}, {-9.92f, -3.54f, 3.4f}, {-3.83f, -9.07f, 3.4f}, {3.83f, -9.07f, 3.4f}, {9.92f, -3.54f, 3.4f},
    };
    const float rad = (float)s->angle_deg / DEG_PER_RAD, c = cosf(rad), sn = sinf(rad);
    for (int i = 0; i < 5; i++) {
        out[i].x = (float)s->x + PAW[i].x * c - PAW[i].y * sn;
        out[i].y = (float)s->y + PAW[i].x * sn + PAW[i].y * c;
        out[i].r = PAW[i].r;
    }
}
