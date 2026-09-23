#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t imu_init(void);
/* Latest acceleration in g. False if the IMU is missing or the read failed. */
bool imu_read_g(float *x, float *y, float *z);
