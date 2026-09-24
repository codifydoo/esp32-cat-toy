#include "pmu.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define PMU_ADDR       0x34
#define PMU_TIMEOUT_MS 50

static const char *TAG = "pmu";
static i2c_master_dev_handle_t s_dev;
static bool s_tried;

bool pmu_init(void)
{
    if (s_tried) {
        return s_dev != NULL;
    }
    s_tried = true;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (i2c_master_probe(bus, PMU_ADDR, PMU_TIMEOUT_MS) != ESP_OK) {
        ESP_LOGW(TAG, "PMU missing: no PWR button and no USB detection");
        return false;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PMU_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "PMU could not be added to the I2C bus");
        s_dev = NULL;
        return false;
    }
    return true;
}

esp_err_t pmu_read(uint8_t reg, uint8_t *val)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, PMU_TIMEOUT_MS);
}

esp_err_t pmu_write(uint8_t reg, uint8_t val)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), PMU_TIMEOUT_MS);
}
