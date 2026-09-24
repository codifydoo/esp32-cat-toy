#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* The AXP2101 power manager on the board support package's I2C bus. pmu_init() adds it once; later
 * calls return the same answer. Reads and writes work from any task (the I2C driver locks the bus)
 * and fail with ESP_ERR_INVALID_STATE when the PMU is missing. */
bool pmu_init(void);
esp_err_t pmu_read(uint8_t reg, uint8_t *val);
esp_err_t pmu_write(uint8_t reg, uint8_t val);
