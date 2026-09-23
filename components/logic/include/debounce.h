#pragma once
#include <stdbool.h>
#include <stdint.h>

#define DEBOUNCE_SAMPLES 2 /* at the 20 ms poll rate: 40 ms */

typedef struct {
    bool stable;
    uint8_t count;
} debounce_t;

void debounce_init(debounce_t *d, bool pressed_now);
/* Feed one raw sample. Returns true exactly once per debounced press (not on release). */
bool debounce_update(debounce_t *d, bool raw_pressed);
