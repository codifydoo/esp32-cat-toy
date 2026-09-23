#pragma once
#include <stdbool.h>
#include <stdint.h>

void idle_init(void);
void idle_wake(void);
bool idle_wake_if_asleep(void);
void idle_power_toggle(void);
uint8_t idle_level(void);
