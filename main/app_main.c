#include "app_manager.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "display.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "sensors.h"
#include "settings.h"
#if CONFIG_CAT_DEV_DIAG
#include "diag.h"
#endif
#if CONFIG_CAT_DEV_GALLERY
#include "gallery.h"
#endif

static const char *TAG = "main";

#if !CONFIG_CAT_DEV_DIAG && !CONFIG_CAT_DEV_GALLERY
static void on_button(button_id_t id)
{
    if (display_lock(-1)) {
        app_manager_on_button(id);
        display_unlock();
    }
}
#endif

void app_main(void)
{
    ESP_ERROR_CHECK(settings_init());
    if (assets_init() != ESP_OK) {
        ESP_LOGE(TAG, "assets missing: images show as magenta dots and sounds are silent");
    }
    ESP_ERROR_CHECK(display_init());
    if (audio_init(settings_volume_cap()) != ESP_OK) {
        ESP_LOGE(TAG, "audio unavailable; running silently");
    }
#if CONFIG_CAT_DEV_DIAG
    diag_start();
#elif CONFIG_CAT_DEV_GALLERY
    cat_set_variant(CAT_VARIANT_ORANGE);
    gallery_start();
#else
    cat_set_variant((cat_variant_t)settings_cat_variant());
    display_lock(-1);
    app_manager_init();
    app_manager_start();
    display_unlock();
    const sensors_listener_t listener = {.on_button = on_button};
    if (sensors_start(&listener) != ESP_OK) {
        ESP_LOGE(TAG, "buttons unavailable");
    }
#endif
}
