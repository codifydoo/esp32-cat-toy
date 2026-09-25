#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Magic paws strokes (design Main.dc.html act('draw'); spec §3.11). */
#define BRUSH_DOT_STEP_PX 7   /* a rainbow dot every 7 px along the stroke */
#define BRUSH_COLOR_EVERY 3   /* the rainbow color steps every 3 dots */
#define BRUSH_PAW_STEP_PX 40  /* a paw print every 40 px */
#define BRUSH_PAW_SIDE_PX 9   /* prints alternate left and right of the stroke */
#define BRUSH_AREA_R      212 /* painted only within this radius of the screen center */
#define BRUSH_COLORS      7   /* the design's rainbow r1..r7 */
#define BRUSH_MAX_STAMPS  32  /* per touch sample; a longer jump restarts the stroke at the new point */

typedef enum { BRUSH_RAINBOW, BRUSH_PAWS } brush_kind_t;

typedef struct {
    int16_t x, y;      /* where to paint: a dot's center, or a paw print's center (already moved to its side) */
    uint8_t color;     /* 0..BRUSH_COLORS-1 */
    int16_t angle_deg; /* paws: turn, 0 = toes up, positive = clockwise on screen */
} stamp_t;

typedef struct {
    brush_kind_t kind;
    bool down;
    float lx, ly;       /* the stroke point of the last stamp */
    uint32_t dots, paws; /* stamps painted since the canvas was last cleared (they pick the colors) */
    int16_t cx, cy;     /* screen center, for BRUSH_AREA_R */
} brush_t;

void brush_init(brush_t *b, brush_kind_t kind, int16_t cx, int16_t cy);
void brush_clear(brush_t *b); /* the canvas was wiped: colors start over */
/* A finger went down: one stamp there (unless outside the area). */
size_t brush_down(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);
/* The finger moved: stamps every BRUSH_DOT_STEP_PX / BRUSH_PAW_STEP_PX along the way from the last one. */
size_t brush_move(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);
void brush_up(brush_t *b);

typedef struct {
    float x, y, r;
} blob_t;
/* The five round parts of a paw print (pad first, then four toes), turned by the stamp's angle. */
void paw_blobs(const stamp_t *s, blob_t out[5]);
