#pragma once
#include "app.h"

void app_manager_init(void);  /* builds the menu screen (LVGL lock held) */
void app_manager_start(void); /* shows the menu */
/* What a (re)start shows (spec §3.7, §3.13): the boot animation, or, after a deep sleep, the lock, the sunrise or
 * the hungry cat at once. Call once from app_main, LVGL lock held, after app_manager_init(). Returns false when
 * the toy is about to sleep for the battery (the hungry cat, or a start at 2 %), so no NTP sync is started; true
 * otherwise. */
bool app_manager_boot(void);
/* true once the start's first screen shows (app_manager_start(), or a deep-sleep wake's lock/sunrise in
 * app_manager_boot()) */
bool app_manager_started(void);
void app_manager_open(const app_t *app);
void app_manager_home(void);  /* destroys the current app and returns to the menu */
const app_t *app_manager_current(void);
