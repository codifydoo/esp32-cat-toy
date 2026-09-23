#include <stdio.h>
#include "assets.h"
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sounds.h"

static const char *TAG = "SMOKE";

static lv_obj_t *show(lv_obj_t *scr, const char *name, lv_align_t align, int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, assets_image(name));
    lv_obj_align(img, align, x, y);
    return img;
}

void app_main(void)
{
    ESP_ERROR_CHECK(assets_init());
    bsp_display_start();
    bsp_display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    show(scr, "cat/orange/M/head", LV_ALIGN_TOP_MID, 0, 30);
    show(scr, "apps/voices", LV_ALIGN_BOTTOM_MID, 0, -40);
    const lv_image_dsc_t *missing = assets_image("does/not/exist");
    show(scr, "does/not/exist", LV_ALIGN_LEFT_MID, 40, 0);
    bsp_display_unlock();

    for (;;) {
        int ok = 0, bad = 0;
        char path[64];
        for (int i = 0; i < SND_COUNT; i++) {
            FILE *f = sounds_path((sound_id_t)i, path, sizeof(path)) ? fopen(path, "rb") : NULL;
            if (f) {
                ok++;
                fclose(f);
            } else {
                bad++;
                ESP_LOGE(TAG, "sound missing: %s", path);
            }
        }
        ESP_LOGI(TAG, "sounds ok=%d missing=%d; placeholder for missing image: %s; psram free %u", ok, bad,
                 missing->header.w == 32 ? "yes" : "no", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
