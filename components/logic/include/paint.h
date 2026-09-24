#pragma once
#include <stdint.h>

/* Painting into an RGB565 picture (Magic paws, spec §3.11). Pure C, no LVGL. */

typedef struct {
    int16_t x0, y0, x1, y1; /* inclusive; empty while x1 < x0 */
} paint_rect_t;

void paint_rect_reset(paint_rect_t *r);
/* 0xRRGGBB -> RGB565 */
uint16_t paint_rgb565(uint32_t rgb);
/* Fills the circle of radius r centered on (cx, cy) with an antialiased edge, blending color at opa
 * (0..255) over what is there. Pixel (x, y) is the square from (x, y) to (x + 1, y + 1). Clipped to the
 * w x h picture; grows *dirty (if not NULL) to cover every pixel it changed. */
void paint_circle(uint16_t *px, int w, int h, float cx, float cy, float r, uint16_t color, uint8_t opa,
                  paint_rect_t *dirty);
