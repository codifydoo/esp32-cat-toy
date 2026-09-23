#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_touch.h"

/* The latest touch reading with up to two points, for the parent gesture. LVGL itself still sees
 * one pointer (the first point). */
typedef struct {
    uint8_t count; /* 0, 1 or 2 fingers */
    int16_t x[2];
    int16_t y[2];
} touch_raw_t;

/* The adapter's custom_touch_read hook (runs on the LVGL task during the indev read). */
esp_err_t touch_raw_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                         uint8_t max_count, void *user_ctx);
/* LVGL task only. The value holds until the next read, which the controller triggers on every
 * change, including the last finger lifting. */
touch_raw_t touch_raw_get(void);
