#include "power.h"
#include "esp_log.h"
#include "pmu.h"

#define PMU_REG_STATUS1 0x00
#define PMU_VBUS_GOOD   (1u << 5) /* XPowersLib isVbusGood(): status 1, bit 5 */

static const char *TAG = "power";

void power_init(void)
{
    if (pmu_init()) {
        ESP_LOGI(TAG, "USB power: %s", power_on_usb() ? "yes" : "no");
    }
}

bool power_on_usb(void)
{
    uint8_t sts = 0;
    return pmu_read(PMU_REG_STATUS1, &sts) == ESP_OK && (sts & PMU_VBUS_GOOD) != 0;
}
