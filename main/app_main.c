#include "assets.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sensors.h"
#include "settings.h"

static const char *TAG = "SMOKE";
static lv_obj_t *s_label;
static uint32_t s_home, s_power;

static void on_button(button_id_t id)
{
    if (id == BUTTON_HOME) {
        s_home++;
    } else {
        s_power++;
    }
    ESP_LOGI(TAG, "button %s", id == BUTTON_HOME ? "HOME" : "POWER");
    if (display_lock(-1)) {
        lv_label_set_text_fmt(s_label, "HOME %lu  POWER %lu", (unsigned long)s_home, (unsigned long)s_power);
        display_unlock();
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(settings_init());
    assets_init();
    ESP_ERROR_CHECK(display_init());
    display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    s_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_28, 0);
    lv_label_set_text(s_label, "press BOOT / PWR");
    lv_obj_center(s_label);
    display_unlock();
    const sensors_listener_t listener = {.on_button = on_button};
    ESP_ERROR_CHECK(sensors_start(&listener));
    for (;;) {
        ESP_LOGI(TAG, "volume cap %u%%", settings_volume_cap());
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
