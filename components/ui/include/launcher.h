#pragma once
#include <stdint.h>

void launcher_create(void);
void launcher_show(void);         /* reloads visible apps, resets the icon, fades the menu in */
void launcher_home_pressed(void); /* Home while already on the menu */
void launcher_set_idle_level(uint8_t level); /* 0 awake, 1 dim, 2 sleeping, 3 off */
void launcher_burn_in_shift(void);
