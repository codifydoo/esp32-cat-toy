#include "paint.h"
#include <math.h>

void paint_rect_reset(paint_rect_t *r) { *r = (paint_rect_t){.x0 = INT16_MAX, .y0 = INT16_MAX, .x1 = -1, .y1 = -1}; }

uint16_t paint_rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800u) | ((rgb >> 5) & 0x07E0u) | ((rgb >> 3) & 0x001Fu));
}

/* a = 0..256 (256 = all src) */
static uint16_t blend(uint16_t dst, uint16_t src, uint32_t a)
{
    const uint32_t na = 256u - a;
    const uint32_t r = ((uint32_t)(src >> 11) * a + (uint32_t)(dst >> 11) * na) >> 8;
    const uint32_t g = ((uint32_t)((src >> 5) & 0x3Fu) * a + (uint32_t)((dst >> 5) & 0x3Fu) * na) >> 8;
    const uint32_t b = ((uint32_t)(src & 0x1Fu) * a + (uint32_t)(dst & 0x1Fu) * na) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void grow(paint_rect_t *r, int x, int y)
{
    if (x < r->x0) {
        r->x0 = (int16_t)x;
    }
    if (y < r->y0) {
        r->y0 = (int16_t)y;
    }
    if (x > r->x1) {
        r->x1 = (int16_t)x;
    }
    if (y > r->y1) {
        r->y1 = (int16_t)y;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void paint_circle(uint16_t *px, int w, int h, float cx, float cy, float r, uint16_t color, uint8_t opa,
                  paint_rect_t *dirty)
{
    if (r <= 0.0f || opa == 0) {
        return;
    }
    const int x0 = clampi((int)floorf(cx - r - 1.0f), 0, w - 1), x1 = clampi((int)ceilf(cx + r + 1.0f), 0, w - 1);
    const int y0 = clampi((int)floorf(cy - r - 1.0f), 0, h - 1), y1 = clampi((int)ceilf(cy + r + 1.0f), 0, h - 1);
    if (cx + r + 1.0f < 0.0f || cy + r + 1.0f < 0.0f || cx - r - 1.0f > (float)(w - 1) ||
        cy - r - 1.0f > (float)(h - 1)) {
        return; /* entirely outside */
    }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cover <= 0.0f) {
                continue;
            }
            if (cover > 1.0f) {
                cover = 1.0f;
            }
            const uint32_t a = (uint32_t)(cover * (float)opa * (256.0f / 255.0f) + 0.5f);
            uint16_t *p = &px[y * w + x];
            *p = blend(*p, color, a > 256u ? 256u : a);
            if (dirty) {
                grow(dirty, x, y);
            }
        }
    }
}
