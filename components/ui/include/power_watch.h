#pragma once

/* The battery screens, the critical sleep and deep sleep (spec §3.13), checked every second on the LVGL task. */
void power_watch_init(void);   /* app_manager_init() calls it */
void power_watch_usb_in(void); /* USB power was just connected: the charging screen, where it may show */
