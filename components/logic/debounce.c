#include "debounce.h"

void debounce_init(debounce_t *d, bool pressed_now)
{
    d->stable = pressed_now;
    d->count = 0;
}

bool debounce_update(debounce_t *d, bool raw_pressed)
{
    if (raw_pressed == d->stable) {
        d->count = 0;
        return false;
    }
    if (++d->count < DEBOUNCE_SAMPLES) {
        return false;
    }
    d->stable = raw_pressed;
    d->count = 0;
    return d->stable;
}
