#include "imu.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "qmi8658.h"

static const char *TAG = "imu";
static qmi8658_dev_t s_dev;
static bool s_ok;

esp_err_t imu_init(void)
{
    ESP_RETURN_ON_ERROR(qmi8658_init(&s_dev, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH), TAG, "init");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_range(&s_dev, QMI8658_ACCEL_RANGE_4G), TAG, "range");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_odr(&s_dev, QMI8658_ACCEL_ODR_125HZ), TAG, "rate");
    ESP_RETURN_ON_ERROR(qmi8658_enable_accel(&s_dev, true), TAG, "enable");
    s_ok = true;
    return ESP_OK;
}

bool imu_read_g(float *x, float *y, float *z)
{
    float mx, my, mz;
    /* qmi8658_read_accel_mg() is declared in qmi8658.h but not implemented in this driver version
     * (link error: undefined reference to `qmi8658_read_accel_mg'). qmi8658_read_accel() returns
     * the same unit (mg) by default: qmi8658_init() leaves dev->accel_unit_mps2 = false and we
     * never call qmi8658_set_accel_unit_mps2/_mg, so the mg-vs-m/s^2 branch inside
     * qmi8658_read_accel() always takes the mg path. */
    if (!s_ok || qmi8658_read_accel(&s_dev, &mx, &my, &mz) != ESP_OK) {
        return false;
    }
    *x = mx / 1000.0f;
    *y = my / 1000.0f;
    *z = mz / 1000.0f;
    return true;
}
