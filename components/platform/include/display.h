#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t display_init(void);
void display_set_brightness(uint8_t pct); /* call on the LVGL task only */
uint8_t display_brightness(void);
bool display_lock(int32_t timeout_ms);    /* -1 waits forever, 0 tries once; recursive */
void display_unlock(void);
