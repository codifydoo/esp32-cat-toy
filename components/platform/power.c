#include "power.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "pmu.h"
#include "sdkconfig.h"

#define PMU_REG_STATUS1   0x00
#define PMU_REG_STATUS2   0x01
#define PMU_REG_GAUGE_CTL 0x18
#define PMU_REG_PERCENT   0xA4
#define PMU_VBUS_GOOD     (1u << 5) /* status 1 (XPowersLib isVbusGood) */
#define PMU_BATTERY_IN    (1u << 3) /* status 1 (isBatteryConnect) */
#define PMU_GAUGE_ON      (1u << 3) /* gauge control (enableGauge) */
#define PMU_DIR_CHARGING  1u        /* status 2, bits 6:5: current flows into the battery (isCharging) */
#define DRAIN_START_PCT   12        /* CONFIG_CAT_DEV_BATTERY_DRAIN */
#define DRAIN_STEP_S      10

static const char *TAG = "power";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static power_state_t s_state = {.pct = -1}; /* under s_lock */

void power_init(void)
{
#if CONFIG_CAT_DEV_IGNORE_USB
    ESP_LOGW(TAG, "dev build: USB power counts as battery");
#endif
#if CONFIG_CAT_DEV_BATTERY_DRAIN
    ESP_LOGW(TAG, "dev build: the battery is faked, draining from %d %%", DRAIN_START_PCT);
#endif
#if CONFIG_CAT_DEV_FAST_SLEEP
    ESP_LOGW(TAG, "dev build: deep sleep 20 s after the panel goes off");
#endif
    if (!pmu_init()) {
        return;
    }
    uint8_t ctl = 0;
    if (pmu_read(PMU_REG_GAUGE_CTL, &ctl) == ESP_OK && !(ctl & PMU_GAUGE_ON)) {
        if (pmu_write(PMU_REG_GAUGE_CTL, ctl | PMU_GAUGE_ON) == ESP_OK) {
            ESP_LOGI(TAG, "fuel gauge switched on");
        } else {
            ESP_LOGW(TAG, "fuel gauge switch-on failed");
        }
    }
    power_poll(true);
    const power_state_t s = power_state();
    ESP_LOGI(TAG, "USB power: %s, battery %d %%, %s", s.usb ? "yes" : "no", s.pct,
             s.charging ? "charging" : "not charging");
}

bool power_on_usb(void)
{
#if CONFIG_CAT_DEV_IGNORE_USB
    return false;
#else
    uint8_t sts = 0;
    return pmu_read(PMU_REG_STATUS1, &sts) == ESP_OK && (sts & PMU_VBUS_GOOD) != 0;
#endif
}

void power_poll(bool with_gauge)
{
    uint8_t s1 = 0, s2 = 0, pct = 0xFF;
    const bool ok = pmu_read(PMU_REG_STATUS1, &s1) == ESP_OK && pmu_read(PMU_REG_STATUS2, &s2) == ESP_OK;
    if (!ok) {
        return; /* unknown, not "unplugged": keep the last known state, or the next good read fakes a USB edge */
    }
    const bool battery = (s1 & PMU_BATTERY_IN) != 0;
    const bool gauge_read = with_gauge && battery && pmu_read(PMU_REG_PERCENT, &pct) == ESP_OK;
#if CONFIG_CAT_DEV_BATTERY_DRAIN
    /* Still empty after a sleep for the battery, so the hungry-cat wake can be checked; a reset starts over. */
    const int64_t steps = esp_timer_get_time() / 1000000 / DRAIN_STEP_S;
    const bool for_battery = deepsleep_was_for_battery();
#endif
    portENTER_CRITICAL(&s_lock);
    s_state.usb = (s1 & PMU_VBUS_GOOD) != 0;
    s_state.charging = ((s2 >> 5) & 3u) == PMU_DIR_CHARGING;
    if (!battery) {
        s_state.pct = -1;
    } else if (gauge_read) {
        s_state.pct = pct <= 100 ? (int8_t)pct : -1;
    }
#if CONFIG_CAT_DEV_IGNORE_USB
    s_state.usb = false;
    s_state.charging = false; /* USB counts as battery here: no charger either */
#endif
#if CONFIG_CAT_DEV_BATTERY_DRAIN
    s_state.pct = for_battery ? 1 : (int8_t)(steps >= DRAIN_START_PCT ? 0 : DRAIN_START_PCT - steps);
    s_state.charging = false;
#endif
    portEXIT_CRITICAL(&s_lock);
}

power_state_t power_state(void)
{
    portENTER_CRITICAL(&s_lock);
    const power_state_t s = s_state;
    portEXIT_CRITICAL(&s_lock);
    return s;
}
