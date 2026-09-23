#include "touch_raw.h"

static touch_raw_t s_last;

esp_err_t touch_raw_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                         uint8_t max_count, void *user_ctx)
{
    (void)user_ctx;
    esp_lcd_touch_point_data_t all[2] = {0};
    uint8_t n = 0;
    esp_lcd_touch_read_data(tp); /* same as the adapter's own read: a failed read leaves no points */
    esp_err_t err = esp_lcd_touch_get_data(tp, all, &n, 2);
    if (err != ESP_OK) {
        n = 0;
    }
    s_last.count = n;
    for (uint8_t i = 0; i < n; i++) {
        s_last.x[i] = (int16_t)all[i].x;
        s_last.y[i] = (int16_t)all[i].y;
    }
    uint8_t give = n < max_count ? n : max_count;
    for (uint8_t i = 0; i < give; i++) {
        points[i] = all[i];
    }
    *count = give;
    return err;
}

touch_raw_t touch_raw_get(void) { return s_last; }
