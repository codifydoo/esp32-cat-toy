#pragma once
#include "app.h"
#include "buttons.h"

void app_manager_init(void);  /* builds the menu screen (LVGL lock held) */
void app_manager_start(void); /* shows the menu */
void app_manager_open(const app_t *app);
void app_manager_home(void);  /* destroys the current app and returns to the menu */
const app_t *app_manager_current(void);
void app_manager_on_button(button_id_t id); /* call with the LVGL lock held */
