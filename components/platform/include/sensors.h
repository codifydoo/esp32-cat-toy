#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    SENSOR_EV_HOME,  /* BOOT pressed */
    SENSOR_EV_POWER, /* PWR short press */
    SENSOR_EV_SHAKE, /* spec §7 shake (motion.h) */
    SENSOR_EV_USB_IN, /* USB power connected (spec §3.13: the charging screen); not at boot */
} sensor_ev_t;

/* Starts the 50 Hz sensors task: the buttons, and the IMU when imu_init() succeeded first. The task
 * only queues events and never waits for the LVGL lock, so a slow frame cannot swallow a quick
 * second button press (spec §3.10). */
esp_err_t sensors_start(void);
/* The next queued event, without waiting; false when there is none (or before sensors_start). */
bool sensors_next(sensor_ev_t *ev);
/* Grip-relative tilt in degrees (motion.h tilt_angles). False without IMU readings. Any task. */
bool sensors_tilt(float *x_deg, float *y_deg);
/* The toy's current angle becomes level (spec §3.10: the grip when an app opens). Any task. */
void sensors_tilt_recenter(void);
