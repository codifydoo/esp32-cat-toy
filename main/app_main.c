#include "assets.h"
#include "audio.h"
#include "display.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "settings.h"
#if CONFIG_CAT_DEV_DIAG
#include "diag.h"
#endif

static const char *TAG = "main";

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
#else
    ESP_LOGW(TAG, "no UI yet: use the diag build until Task 15");
#endif
}
