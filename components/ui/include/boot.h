#pragma once
/* Shows the waking cat inside two expanding rings for BOOT_MS, then calls done (LVGL lock held). */
void boot_run(void (*done)(void));
