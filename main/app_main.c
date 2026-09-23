#include <dirent.h>
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/assets",
        .partition_label = "assets",
        .read_only = true,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    bsp_display_start();
    bsp_display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_t *dot = lv_obj_create(scr);
    lv_obj_set_size(dot, 200, 200);
    lv_obj_center(dot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xFF9F43), 0);
    bsp_display_unlock();

    for (;;) {
        DIR *dir = opendir("/assets");
        struct dirent *e;
        while (dir && (e = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "asset: %s", e->d_name);
        }
        if (dir) {
            closedir(dir);
        }
        ESP_LOGI(TAG, "psram free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
