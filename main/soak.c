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

#define SOAK_ROUNDS        50
#define SOAK_STEP_MS       500
#define SOAK_TOLERANCE     4096
#define SOAK_WARMUP_ROUNDS 3

static const char *TAG = "SOAK";

static void step(const app_t *app)
{
    display_lock(-1);
    lv_display_trigger_activity(NULL); /* keep idle dimming out of the measurement */
    app_manager_open(app);
    if (app->on_shake) {
        app->on_shake(); /* a reaction that Home cuts short must still clean up after itself */
    }
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS / 2));
    display_lock(-1);
    if (app->on_tilt) {
        app->on_tilt(20.0f, 20.0f); /* shows and rolls the ball */
    }
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS / 2));
    display_lock(-1);
    app_manager_home();
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS));
}

static void insertion_sort(size_t *a, int n)
{
    for (int i = 1; i < n; i++) {
        size_t v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = v;
    }
}

/* Free heap as the median of 10 readings 50 ms apart. Single readings swing both ways: a display
 * flush briefly holds ~25 KB of internal RAM, and a rare state frees ~22.5 KB; the median ignores
 * both, and a real leak still moves it. */
static void free_heap(size_t *internal, size_t *psram)
{
    size_t in[10], ps[10];
    for (int i = 0; i < 10; i++) {
        in[i] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ps[i] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    insertion_sort(in, 10);
    insertion_sort(ps, 10);
    *internal = in[5];
    *psram = ps[5];
}

static void soak_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(6000)); /* boot animation */
    /* warm-up: fills the image and sound caches. First opens load images and sounds, and the
     * piano's first open blocks the UI long enough that one round can miss animation frames that
     * are then cached forever, so several rounds are needed to reliably reach every frame. */
    for (int w = 0; w < SOAK_WARMUP_ROUNDS; w++) {
        for (size_t i = 0; i < APP_COUNT; i++) {
            step(APPS[i]);
        }
    }
    size_t base_int, base_ps;
    free_heap(&base_int, &base_ps);
    ESP_LOGI(TAG, "baseline internal=%u psram=%u", (unsigned)base_int, (unsigned)base_ps);
    for (int r = 0; r < SOAK_ROUNDS; r++) {
        for (size_t i = 0; i < APP_COUNT; i++) {
            step(APPS[i]);
        }
        if ((r + 1) % 10 == 0) {
            size_t in, ps;
            free_heap(&in, &ps);
            ESP_LOGI(TAG, "round %d internal=%u psram=%u", r + 1, (unsigned)in, (unsigned)ps);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    size_t end_int, end_ps;
    free_heap(&end_int, &end_ps);
    long d_int = (long)end_int - (long)base_int;
    long d_ps = (long)end_ps - (long)base_ps;
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
