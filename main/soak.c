#include "sdkconfig.h"
#if CONFIG_CAT_DEV_SOAK
#include "soak.h"
#include <stdbool.h>
#include <stdlib.h>
#include "app.h"
#include "app_manager.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SOAK_ROUNDS    50
#define SOAK_STEP_MS   500
#define SOAK_TOLERANCE 4096

static const char *TAG = "SOAK";

static void step(const app_t *app)
{
    display_lock(-1);
    lv_display_trigger_activity(NULL); /* keep idle dimming out of the measurement */
    app_manager_open(app);
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS));
    display_lock(-1);
    app_manager_home();
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS));
}

static void soak_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(6000)); /* boot animation */
    for (size_t i = 0; i < APP_COUNT; i++) {
        step(APPS[i]); /* warm-up: fills the image and sound caches */
    }
    size_t base_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t base_ps = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "baseline internal=%u psram=%u", (unsigned)base_int, (unsigned)base_ps);
    for (int r = 0; r < SOAK_ROUNDS; r++) {
        for (size_t i = 0; i < APP_COUNT; i++) {
            step(APPS[i]);
        }
        if ((r + 1) % 10 == 0) {
            ESP_LOGI(TAG, "round %d internal=%u psram=%u", r + 1,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    long d_int = (long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) - (long)base_int;
    long d_ps = (long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) - (long)base_ps;
    bool pass = labs(d_int) <= SOAK_TOLERANCE && labs(d_ps) <= SOAK_TOLERANCE;
    ESP_LOGI(TAG, "%s: internal delta=%ld psram delta=%ld over %d rounds", pass ? "PASS" : "FAIL", d_int, d_ps,
             SOAK_ROUNDS);
    vTaskDelete(NULL);
}

void soak_start(void)
{
    xTaskCreatePinnedToCore(soak_task, "soak", 8192, NULL, 3, NULL, 0);
}
#endif
