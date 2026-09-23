#include "sensors.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SENSORS_PERIOD_MS 20

static const char *TAG = "sensors";
static sensors_listener_t s_listener;

static void sensors_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint8_t ev = buttons_poll();
        if ((ev & BUTTON_EV_HOME) && s_listener.on_button) {
            s_listener.on_button(BUTTON_HOME);
        }
        if ((ev & BUTTON_EV_POWER) && s_listener.on_button) {
            s_listener.on_button(BUTTON_POWER);
        }
        /* A callback above may have blocked on display_lock for a while. Re-base here instead of
         * letting vTaskDelayUntil "catch up" on the missed periods, which would fire the next
         * few polls back-to-back and collapse the ~20 ms spacing the debounce filter relies on.
         * In the normal (non-blocking) case this still keeps a steady ~20 ms period. */
        last = xTaskGetTickCount();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSORS_PERIOD_MS));
    }
}

esp_err_t sensors_start(const sensors_listener_t *listener)
{
    s_listener = *listener;
    ESP_RETURN_ON_ERROR(buttons_init(), TAG, "buttons");
    BaseType_t ok = xTaskCreatePinnedToCore(sensors_task, "sensors", 8192, NULL, 4, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
