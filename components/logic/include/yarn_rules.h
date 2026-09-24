#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Yarn roll rules (design Screen.dc.html yarn view and Main.dc.html tilt(); spec §3.11). */
#define YARN_RANGE     185 /* the yarn's center stays within this radius of the screen center */
#define YARN_CHASE_PX  92  /* the cat stays this far behind the yarn */
#define YARN_WIN_PX    62  /* the yarn is in when its center is this close to the basket's mouth */
#define YARN_BASKETS   4
#define YARN_FOLLOW_MS 130 /* the cat closes 63 % of the gap in this time: the design's 400 ms ease-out */

/* Center of basket i (i wraps): the design's four spots, used round after round. */
void yarn_basket(uint8_t i, int16_t *x, int16_t *y);
/* Where the chasing cat's center goes: YARN_CHASE_PX from the yarn toward (cx, cy), or straight above the
 * yarn when the yarn is within 30 px of (cx, cy). */
void yarn_chase_target(float yx, float yy, float cx, float cy, float *tx, float *ty);
/* True when the yarn's center is within YARN_WIN_PX of the point 10 px above the basket's center. */
bool yarn_in_basket(float yx, float yy, int16_t bx, int16_t by);
/* One ease-out step from pos toward target: after dt_ms the gap has shrunk by exp(-dt_ms / YARN_FOLLOW_MS). */
float yarn_follow(float pos, float target, uint32_t dt_ms);
