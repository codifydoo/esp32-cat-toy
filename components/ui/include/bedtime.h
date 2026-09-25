#pragma once
#include <stdbool.h>

/* The bedtime lock and the sunrise (spec §3.8, §3.12). LVGL task only. */
void bedtime_init(void); /* starts the 1 s schedule check; app_manager_init() calls it */
/* Called where the menu would show next (the boot animation ended, an app closed). While it is night it
 * shows the lock instead, deleting the active screen, and returns true. Otherwise it forgets a lock the
 * parent area moved out of the night, and returns false. */
bool bedtime_take_over(void);
/* True while the lock or the sunrise is on screen (not while the parent area is open over it). */
bool bedtime_showing(void);
/* The lock's panel is lit (the sunrise always is). Meaningful while bedtime_showing(). */
bool bedtime_panel_lit(void);
/* PWR while bedtime_showing(): the lock's panel off or back on. During the sunrise: the menu, dark (with
 * CONFIG_CAT_HOME_ON_POWER, PWR is Home: the menu, lit). */
void bedtime_power(void);
/* Home while bedtime_showing(): nothing on the lock (spec §3.12); the sunrise goes straight to the menu,
 * without the mew (spec §1: only the lock ignores Home). */
void bedtime_home(void);
