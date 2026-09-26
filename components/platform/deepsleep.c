#include "deepsleep.h"
#include "audio.h"
#include "bsp/esp-bsp.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define WAKE_BOOT_GPIO  GPIO_NUM_0
#define WAKE_TOUCH_GPIO BSP_LCD_TOUCH_INT /* GPIO11: low while the CST9217 reports a touch */
#define WAKE_PWR_GPIO   GPIO_NUM_NC       /* the AXP2101 interrupt, if the M7 power diagnostics found a pin */

static const char *TAG = "deepsleep";
RTC_DATA_ATTR static bool s_for_battery; /* survives deep sleep: set by deepsleep_start(), for the NEXT sleep */
static deepsleep_cause_t s_cause;
static bool s_woke_for_battery; /* this wake's answer, latched in deepsleep_init() before deepsleep_start()
                                  * can overwrite s_for_battery for the next sleep */

void deepsleep_init(void)
{
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT1: s_cause = DEEPSLEEP_BY_PIN; break;
#if CONFIG_CAT_DEV_AUTO_WAKE
    case ESP_SLEEP_WAKEUP_TIMER: s_cause = DEEPSLEEP_BY_PIN; break; /* the auto-wake dev build: a stand-in touch */
#else
    case ESP_SLEEP_WAKEUP_TIMER: s_cause = DEEPSLEEP_BY_TIMER; break;
#endif
    default: s_cause = DEEPSLEEP_COLD; break;
    }
    if (s_cause == DEEPSLEEP_COLD) {
        s_for_battery = false;
    }
    s_woke_for_battery = s_cause != DEEPSLEEP_COLD && s_for_battery;
    gpio_hold_dis(BSP_POWER_AMP_IO); /* the amplifier pin must follow the codec driver again */
    gpio_deep_sleep_hold_dis();
    static const char *const NAMES[] = {"cold start", "woke by a pin", "woke by the timer"};
    ESP_LOGI(TAG, "%s%s", NAMES[s_cause], s_woke_for_battery ? " (slept for the battery)" : "");
}

deepsleep_cause_t deepsleep_cause(void) { return s_cause; }

bool deepsleep_was_for_battery(void) { return s_woke_for_battery; }

static void wake_pin(gpio_num_t pin, uint64_t *mask)
{
    if (pin != GPIO_NUM_NC) {
        *mask |= BIT64(pin);
    }
}

static void log_if_failed(esp_err_t err, const char *what)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
    }
}

void deepsleep_start(uint32_t timer_s, bool for_battery)
{
#if CONFIG_CAT_DEV_AUTO_WAKE
    timer_s = 10; /* the auto-wake dev build: the next wake comes by itself, and deepsleep_init() calls it a touch */
#endif
    ESP_LOGI(TAG, "deep sleep: timer %lu s%s", (unsigned long)timer_s, for_battery ? ", for the battery" : "");
    s_for_battery = for_battery;
    audio_stop_all();
    display_sleep();
    gpio_set_level(BSP_POWER_AMP_IO, 0); /* the speaker amplifier stays off while the chip sleeps */
    gpio_hold_en(BSP_POWER_AMP_IO);
    gpio_deep_sleep_hold_en();
    uint64_t mask = 0;
    wake_pin(WAKE_BOOT_GPIO, &mask);
    wake_pin(WAKE_TOUCH_GPIO, &mask);
    wake_pin(WAKE_PWR_GPIO, &mask);
    log_if_failed(esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW), "ext1 wakeup");
    /* the RTC peripherals stay powered: they keep the pull-ups on in sleep */
    log_if_failed(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON), "rtc periph power");
    for (int pin = 0; pin < GPIO_NUM_MAX; pin++) {
        if (mask & BIT64(pin)) {
            log_if_failed(rtc_gpio_pullup_en((gpio_num_t)pin), "rtc pullup");
            log_if_failed(rtc_gpio_pulldown_dis((gpio_num_t)pin), "rtc pulldown");
        }
    }
    if (timer_s > 0) {
        log_if_failed(esp_sleep_enable_timer_wakeup((uint64_t)timer_s * 1000000), "timer wakeup");
    }
    vTaskDelay(pdMS_TO_TICKS(100)); /* the log line and the panel's sleep command drain */
    esp_deep_sleep_start();
}
