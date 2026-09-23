#include "carousel.h"

void carousel_init(carousel_t *c, uint8_t count)
{
    c->count = count ? count : 1;
    c->index = 0;
}

uint8_t carousel_step(carousel_t *c, int8_t dir)
{
    if (dir > 0) {
        c->index = (uint8_t)((c->index + 1) % c->count);
    } else if (dir < 0) {
        c->index = (uint8_t)((c->index + c->count - 1) % c->count);
    }
    return c->index;
}

dot_window_t carousel_dot_window(const carousel_t *c)
{
    dot_window_t w = {.first = 0, .n = c->count};
    if (c->count > CAROUSEL_MAX_DOTS) {
        int first = (int)c->index - CAROUSEL_MAX_DOTS / 2;
        int max_first = c->count - CAROUSEL_MAX_DOTS;
        if (first < 0) {
            first = 0;
        }
        if (first > max_first) {
            first = max_first;
        }
        w.first = (uint8_t)first;
        w.n = CAROUSEL_MAX_DOTS;
    }
    return w;
}

dot_kind_t carousel_dot_kind(const carousel_t *c, uint8_t i)
{
    dot_window_t w = carousel_dot_window(c);
    int last = w.first + w.n - 1;
    if (i < w.first || i > last) {
        return DOT_HIDDEN;
    }
    if (i == c->index) {
        return DOT_ACTIVE;
    }
    if (c->count > CAROUSEL_MAX_DOTS && ((i == w.first && w.first > 0) || (i == last && last < c->count - 1))) {
        return DOT_EDGE;
    }
    return DOT_IDLE;
}
