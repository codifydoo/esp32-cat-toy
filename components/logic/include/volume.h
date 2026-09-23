#pragma once
#include <stdint.h>

#define VOLUME_MIN  10 /* never fully silent: a silent toy looks broken to her */
#define VOLUME_MAX  100
#define VOLUME_STEP 10

/* The parent screen's -/+ step: the next multiple of 10 below (dir < 0) or above (dir > 0),
 * kept within VOLUME_MIN..VOLUME_MAX. A value outside that range is clamped into it instead. */
uint8_t volume_step(uint8_t current, int dir);
