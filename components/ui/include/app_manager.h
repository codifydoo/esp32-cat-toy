#pragma once
#include "app.h"

void app_manager_init(void);  /* builds the menu screen (LVGL lock held) */
void app_manager_start(void); /* shows the menu */
bool app_manager_started(void); /* true once the boot animation has ended (app_manager_start) */
void app_manager_open(const app_t *app);
void app_manager_home(void);  /* destroys the current app and returns to the menu */
const app_t *app_manager_current(void);
