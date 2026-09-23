#include "touch_raw.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_report.h"

static touch_raw_t s_last;

/* Reads the controller's report the way the CST9217 driver does: the 16-bit register address,
 * a 2 ms pause, then the data. */
static esp_err_t read_report(esp_lcd_touch_handle_t tp, uint8_t *d, size_t len)
{
    const uint8_t lo = TOUCH_REPORT_REG & 0xFF;
    esp_err_t err = esp_lcd_panel_io_tx_param(tp->io, TOUCH_REPORT_REG >> 8, &lo, 1);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = esp_lcd_panel_io_rx_param(tp->io, -1, d, len);
    }
    return err;
}

esp_err_t touch_raw_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                         uint8_t max_count, void *user_ctx)
{
    (void)user_ctx;
    /* LVGL's point: the driver's own read, exactly as the adapter does it without this hook. */
    esp_lcd_touch_read_data(tp);
    esp_err_t err = esp_lcd_touch_get_data(tp, points, count, max_count);

    /* The finger count: the Waveshare CST9217 driver stops at one point, so while anything is
     * touching, read the whole report again for the count and the second point. */
    uint8_t d[TOUCH_REPORT_LEN] = {0};
    int16_t x[2], y[2];
    uint8_t n = 0;
    if (err == ESP_OK && *count > 0 && read_report(tp, d, sizeof(d)) == ESP_OK) {
        n = touch_report_parse(d, sizeof(d), x, y);
    }
    s_last.count = n;
    for (uint8_t i = 0; i < n; i++) {
        s_last.x[i] = x[i];
        s_last.y[i] = y[i];
    }
    return err;
}

touch_raw_t touch_raw_get(void) { return s_last; }
