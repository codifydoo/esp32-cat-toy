#include "sensors.h"
#include "buttons.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu.h"
#include "motion.h"

#define SENSORS_PERIOD_MS 20 /* 50 Hz: button debounce, shake and tilt */
#define EVENT_QUEUE_LEN   8

/* The QMI8658's axes in screen terms, measured on the device (docs/device-checks.md, "Serial
 * listener and accelerometer axes (M4)"), confirmed by the Pet the cat walkthrough (the ball must
 * roll downhill): screen x = −ax, screen y = ay, and each reads +1 g when it points up. Raw z is
 * already out of the glass (+1 g flat, face up). */
#define SCREEN_X(ax, ay) (-(ax))
#define SCREEN_Y(ax, ay) (ay)

static const char *TAG = "sensors";
static QueueHandle_t s_events;
static portMUX_TYPE s_tilt_lock = portMUX_INITIALIZER_UNLOCKED;
static tilt_t s_tilt;   /* shared with sensors_tilt*(): only under s_tilt_lock */
static shake_t s_shake; /* sensors task only */

static void post(sensor_ev_t ev)
{
    if (xQueueSend(s_events, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped event %d", (int)ev);
    }
}

static void sample_imu(void)
{
    float ax, ay, az;
    if (!imu_read_g(&ax, &ay, &az)) {
        return; /* no IMU: shake and tilt do nothing (spec §4.7) */
    }
    const float x = SCREEN_X(ax, ay), y = SCREEN_Y(ax, ay), z = az;
    if (shake_update(&s_shake, x, y, z, (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()))) {
        post(SENSOR_EV_SHAKE);
    }
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_feed(&s_tilt, x, y, z);
    taskEXIT_CRITICAL(&s_tilt_lock);
}

static void sensors_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* Re-based every loop: after a slow I2C read (a PMU timeout is 50 ms) the next poll waits a
         * full period instead of firing back-to-back, which the debounce's 20 ms spacing relies on. */
        TickType_t last = xTaskGetTickCount();
        uint8_t ev = buttons_poll();
        if (ev & BUTTON_EV_HOME) {
            post(SENSOR_EV_HOME);
        }
        if (ev & BUTTON_EV_POWER) {
            post(SENSOR_EV_POWER);
        }
        sample_imu();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSORS_PERIOD_MS));
    }
}

esp_err_t sensors_start(void)
{
    ESP_RETURN_ON_ERROR(buttons_init(), TAG, "buttons");
    tilt_reset(&s_tilt);
    shake_reset(&s_shake);
    s_events = xQueueCreate(EVENT_QUEUE_LEN, sizeof(sensor_ev_t));
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "queue");
    BaseType_t ok = xTaskCreatePinnedToCore(sensors_task, "sensors", 8192, NULL, 4, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool sensors_next(sensor_ev_t *ev)
{
    return s_events && xQueueReceive(s_events, ev, 0) == pdTRUE;
}

bool sensors_tilt(float *x_deg, float *y_deg)
{
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_t t = s_tilt;
    taskEXIT_CRITICAL(&s_tilt_lock);
    return tilt_angles(&t, x_deg, y_deg);
}

void sensors_tilt_recenter(void)
{
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_recenter(&s_tilt);
    taskEXIT_CRITICAL(&s_tilt_lock);
}
