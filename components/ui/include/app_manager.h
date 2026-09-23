#pragma once
#include "app.h"
#include "buttons.h"

void app_manager_init(void);  /* builds the menu screen (LVGL lock held) */
void app_manager_start(void); /* shows the menu */
void app_manager_open(const app_t *app);
void app_manager_home(void);  /* destroys the current app and returns to the menu */
const app_t *app_manager_current(void);
/* Call with the LVGL lock held (e.g. from the button-poll task, under display_lock()); queues
 * the press to be handled on the LVGL task itself via lv_async_call(), since the handling can
 * change display brightness, which display.h requires to happen only on the LVGL task. */
void app_manager_on_button(button_id_t id);
