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
