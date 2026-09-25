#pragma once
#include <stdbool.h>
#include <stdint.h>

void idle_init(void);
void idle_wake(void);
bool idle_wake_if_asleep(void);
void idle_power_toggle(void);
uint8_t idle_level(void);
/* The bedtime lock takes the panel: back to awake with no dimming and no touch catcher. The brightness
 * is left to the caller. */
void idle_hand_over(void);
