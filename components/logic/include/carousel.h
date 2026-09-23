#pragma once
#include <stdint.h>

#define CAROUSEL_MAX_DOTS 7

typedef struct {
    uint8_t count;
    uint8_t index;
} carousel_t;

typedef struct {
    uint8_t first; /* first app index that has a visible dot */
    uint8_t n;     /* number of visible dots */
} dot_window_t;

typedef enum { DOT_HIDDEN, DOT_IDLE, DOT_ACTIVE, DOT_EDGE } dot_kind_t;

void carousel_init(carousel_t *c, uint8_t count);
uint8_t carousel_step(carousel_t *c, int8_t dir);
dot_window_t carousel_dot_window(const carousel_t *c);
dot_kind_t carousel_dot_kind(const carousel_t *c, uint8_t app_index);
