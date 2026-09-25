#include "app_manager.h"
#include "assets.h"
#include "audio.h"
#include "boot.h"
#include "cat.h"
#include "clock.h"
#include "deepsleep.h"
#include "display.h"
#include "esp_log.h"
#include "esp_system.h"
#include "imu.h"
#include "lvgl.h"
#include "power.h"
#include "sdkconfig.h"
#include "sensors.h"
#include "settings.h"
#if CONFIG_CAT_DEV_DIAG
#include "diag.h"
#endif
#if CONFIG_CAT_DEV_GALLERY
#include "gallery.h"
#endif
#if CONFIG_CAT_DEV_SOAK
#include "soak.h"
#endif
#if CONFIG_CAT_DEV_POWER_DIAG
#include "power_diag.h"
#endif

static const char *TAG = "main";

void app_main(void)
{
    deepsleep_init();
    /* The ROM's own reset-reason line is often lost over USB, so log it again (esp_reset_reason_t:
     * 1 power-on, 3 software, 4 panic, 5-7 watchdogs, 9 brownout, 11 USB, 14 power glitch). */
    ESP_LOGI(TAG, "reset reason %d", (int)esp_reset_reason());
    ESP_ERROR_CHECK(settings_init());
    if (clock_init() != ESP_OK) {
        ESP_LOGE(TAG, "network stack unavailable: no NTP");
    }
    if (assets_init() != ESP_OK) {
        ESP_LOGE(TAG, "assets missing: images show as magenta dots and sounds are silent");
    }
    ESP_ERROR_CHECK(display_init());
    /* Black out the default LVGL screen before anything else loads, so the panel doesn't flash
     * white while the boot screen is being built. */
    if (display_lock(-1)) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        display_unlock();
    }
    if (audio_init(settings_volume_cap()) != ESP_OK) {
        ESP_LOGE(TAG, "audio unavailable; running silently");
    }
#if CONFIG_CAT_DEV_DIAG
    diag_start();
#elif CONFIG_CAT_DEV_GALLERY
    cat_set_variant(CAT_VARIANT_ORANGE);
    gallery_start();
#elif CONFIG_CAT_DEV_POWER_DIAG
    power_diag_start();
#else
    power_init();
    cat_set_variant((cat_variant_t)settings_cat_variant());
    display_lock(-1);
    app_manager_init();
    app_manager_boot();
    display_unlock();
    if (imu_init() != ESP_OK) {
        ESP_LOGE(TAG, "IMU unavailable: shake and tilt do nothing");
    }
    if (sensors_start() != ESP_OK) {
        ESP_LOGE(TAG, "sensors unavailable: no buttons, shake or tilt");
    }
    clock_sync_start(); /* NTP at boot, in the background (spec §3.2) */
#if CONFIG_CAT_DEV_SOAK
    soak_start();
#endif
#endif
}
