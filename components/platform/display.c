#include "display.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"

static const char *TAG = "display";
static uint8_t s_brightness = 100;

esp_err_t display_init(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
        .touch_flags = {.swap_xy = 0, .mirror_x = 1, .mirror_y = 1}, /* same as bsp_display_start() */
    };
    cfg.lv_adapter_cfg.task_core_id = 1; /* audio and sensors run on core 0 */
    ESP_RETURN_ON_FALSE(bsp_display_start_with_config(&cfg) != NULL, ESP_FAIL, TAG, "display start failed");
    return ESP_OK;
}

void display_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    if (bsp_display_brightness_set(pct) == ESP_OK) {
        s_brightness = pct;
    }
}

uint8_t display_brightness(void) { return s_brightness; }
bool display_lock(int32_t timeout_ms) { return esp_lv_adapter_lock(timeout_ms) == ESP_OK; }
void display_unlock(void) { esp_lv_adapter_unlock(); }
