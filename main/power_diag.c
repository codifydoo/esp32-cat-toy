#include "sdkconfig.h"
#if CONFIG_CAT_DEV_POWER_DIAG
#include "power_diag.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include "bsp/esp-bsp.h"
#include "buttons.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pmu.h"

/* M7 Task 1: the hardware facts deep sleep and the battery screens rest on (docs/device-checks.md, "Power
 * diagnostics (M7)"). Round 0 (a cold start) watches the gauge, the touch INT and the free RTC pins for
 * WATCH_S while the user presses PWR and touches the screen, then sleeps TIMER_SLEEP_S on the timer alone.
 * Round 1 expects that timer wake and sleeps until a touch; round 2 until BOOT; round 3 until PWR, only if
 * round 0 saw a pin follow PWR. Rounds 1-3 also wake by themselves after FALLBACK_S. */

#define WATCH_S       40
#define TIMER_SLEEP_S 180
#define FALLBACK_S    300
#define READ_S        8 /* each message stays this long before the screen goes dark */

#define REG_STATUS1   0x00 /* bit 5 VBUS good, bit 3 battery present */
#define REG_STATUS2   0x01 /* bits 6:5 current direction (1 charging), bits 2:0 charger state */
#define REG_GAUGE_CTL 0x18 /* bit 3 fuel gauge on (XPowersLib enableGauge) */
#define REG_ADC_EN    0x30 /* bit 0 battery voltage ADC */
#define REG_VBAT_H    0x34 /* battery voltage in mV: 6 high bits ... */
#define REG_VBAT_L    0x35 /* ... and 8 low bits */
#define REG_PERCENT   0xA4 /* fuel gauge, 0-100 % */

static const gpio_num_t CANDIDATES[] = {GPIO_NUM_3, GPIO_NUM_13, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_21};
#define N_CAND (sizeof(CANDIDATES) / sizeof(CANDIDATES[0]))

static const char *TAG = "PWRDIAG";
RTC_DATA_ATTR static uint32_t s_round;
RTC_DATA_ATTR static int64_t s_sleep_at_us; /* gettimeofday() at sleep entry: RTC time across the sleep */
RTC_DATA_ATTR static uint32_t s_pwr_pins;   /* bit i: CANDIDATES[i] was low at every PWR press in round 0 */
static lv_obj_t *s_label;
static volatile uint32_t s_int_edges;

static void say(const char *fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "screen: %s", buf);
    if (display_lock(-1)) {
        lv_label_set_text(s_label, buf);
        display_unlock();
    }
}

static uint8_t reg(uint8_t r)
{
    uint8_t v = 0xFF;
    pmu_read(r, &v);
    return v;
}

static void log_power(void)
{
    const uint8_t s1 = reg(REG_STATUS1), s2 = reg(REG_STATUS2);
    const unsigned mv = ((reg(REG_VBAT_H) & 0x3Fu) << 8) | reg(REG_VBAT_L);
    ESP_LOGI(TAG, "status1 0x%02x (vbus %d, battery %d) status2 0x%02x (direction %d, charger %d) gauge_ctl 0x%02x "
                  "adc 0x%02x percent %u vbat %u mV",
             s1, (s1 >> 5) & 1, (s1 >> 3) & 1, s2, (s2 >> 5) & 3, s2 & 7, reg(REG_GAUGE_CTL), reg(REG_ADC_EN),
             reg(REG_PERCENT), mv);
}

static void IRAM_ATTR int_edge(void *arg)
{
    (void)arg;
    s_int_edges++;
}

static int64_t rtc_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Round 0: the gauge, the touch INT and which free RTC pin (if any) follows PWR. */
static void watch(void)
{
    say("Round 0: press PWR 3 times, a few seconds apart, and touch the screen a few times. %d s.", WATCH_S);
    const uint8_t ctl = reg(REG_GAUGE_CTL);
    ESP_LOGI(TAG, "gauge %s at start (0x18 = 0x%02x)", (ctl & 0x08) ? "on" : "off", ctl);
    log_power();
    if (!(ctl & 0x08)) {
        pmu_write(REG_GAUGE_CTL, ctl | 0x08);
        ESP_LOGI(TAG, "gauge switched on");
    }
    uint8_t prev[N_CAND];
    uint32_t low_at_start = 0, followed = UINT32_MAX, presses = 0;
    for (size_t i = 0; i < N_CAND; i++) {
        const gpio_config_t c = {.pin_bit_mask = BIT64(CANDIDATES[i]), .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
        gpio_config(&c);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    for (size_t i = 0; i < N_CAND; i++) {
        prev[i] = (uint8_t)gpio_get_level(CANDIDATES[i]);
        low_at_start |= prev[i] ? 0 : 1u << i;
        ESP_LOGI(TAG, "gpio%d starts %d", CANDIDATES[i], prev[i]);
    }
    gpio_install_isr_service(0); /* ESP_ERR_INVALID_STATE if already installed: fine */
    gpio_set_intr_type(BSP_LCD_TOUCH_INT, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(BSP_LCD_TOUCH_INT, int_edge, NULL);
    gpio_intr_enable(BSP_LCD_TOUCH_INT);
    buttons_init();
    const int64_t end = esp_timer_get_time() + (int64_t)WATCH_S * 1000000;
    int64_t next_log = 0;
    while (esp_timer_get_time() < end) {
        uint32_t low_now = 0;
        for (size_t i = 0; i < N_CAND; i++) { /* sampled before buttons_poll() clears the PWR interrupt */
            const uint8_t lvl = (uint8_t)gpio_get_level(CANDIDATES[i]);
            if (lvl != prev[i]) {
                ESP_LOGI(TAG, "gpio%d -> %d", CANDIDATES[i], lvl);
                prev[i] = lvl;
            }
            low_now |= lvl ? 0 : 1u << i;
        }
        if (buttons_poll() & BUTTON_EV_POWER) {
            presses++;
            followed &= low_now;
            ESP_LOGI(TAG, "PWR press %" PRIu32 ": candidates low 0x%02" PRIx32, presses, low_now);
        }
        if (esp_timer_get_time() >= next_log) {
            log_power();
            ESP_LOGI(TAG, "touch INT edges so far: %" PRIu32, s_int_edges);
            next_log = esp_timer_get_time() + 2000000;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    gpio_isr_handler_remove(BSP_LCD_TOUCH_INT);
    s_pwr_pins = presses ? followed & ~low_at_start : 0;
    ESP_LOGI(TAG, "round 0 done: %" PRIu32 " PWR presses, pins that followed PWR 0x%02" PRIx32 ", touch INT edges %" PRIu32,
             presses, s_pwr_pins, s_int_edges);
}

static void sleep_now(uint32_t seconds, bool pins, uint32_t pwr_bits, const char *text)
{
    say("%s (the screen goes dark in %d s)", text, READ_S);
    vTaskDelay(pdMS_TO_TICKS(READ_S * 1000));
    uint64_t mask = 0;
    if (pins) {
        mask = BIT64(GPIO_NUM_0) | BIT64(BSP_LCD_TOUCH_INT);
        for (size_t i = 0; i < N_CAND; i++) {
            if (pwr_bits & (1u << i)) {
                mask |= BIT64(CANDIDATES[i]);
            }
        }
        for (int pin = 0; pin < 22; pin++) {
            if (mask & BIT64(pin)) {
                const int level = gpio_get_level((gpio_num_t)pin);
                ESP_LOGI(TAG, "gpio%d is %d at sleep entry%s", pin, level, level ? "" : " (low: it may wake at once)");
            }
        }
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON)); /* keeps the pull-ups */
        for (int pin = 0; pin < 22; pin++) {
            if (mask & BIT64(pin)) {
                rtc_gpio_pullup_en((gpio_num_t)pin);
                rtc_gpio_pulldown_dis((gpio_num_t)pin);
            }
        }
    }
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000));
    s_sleep_at_us = rtc_now_us();
    ESP_LOGI(TAG, "sleeping: round %" PRIu32 ", timer %" PRIu32 " s, ext1 pins 0x%" PRIx64, s_round, seconds, mask);
    display_lock(-1); /* kept: nothing may draw again before the sleep */
    display_sleep();
    vTaskDelay(pdMS_TO_TICKS(100)); /* the log line drains */
    esp_deep_sleep_start();
}

static void diag_task(void *arg)
{
    (void)arg;
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        s_round = 0; /* a cold start or reset: RTC memory was reset too */
    } else {
        s_round++;
        const uint64_t pins = cause == ESP_SLEEP_WAKEUP_EXT1 ? esp_sleep_get_ext1_wakeup_status() : 0;
        const double slept = (double)(rtc_now_us() - s_sleep_at_us) / 1e6;
        ESP_LOGI(TAG, "woke: cause %d (%s), ext1 pins 0x%" PRIx64 ", slept %.1f s by the RTC", (int)cause,
                 cause == ESP_SLEEP_WAKEUP_TIMER ? "timer" : cause == ESP_SLEEP_WAKEUP_EXT1 ? "pin" : "other", pins,
                 slept);
        say("Woke by %s (pins 0x%llx) after %.0f s.", cause == ESP_SLEEP_WAKEUP_TIMER ? "the timer" : "a pin",
            (unsigned long long)pins, slept);
        for (int i = 0; i < 10; i++) { /* host time minus uptime = boot time, for the drift (serial_log -t) */
            ESP_LOGI(TAG, "alive, uptime %lld ms", (long long)(esp_timer_get_time() / 1000));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    switch (s_round) {
    case 0:
        watch();
        sleep_now(TIMER_SLEEP_S, false, 0, "Round 1: sleeping 3 minutes. Don't touch anything: it wakes by itself.");
        break;
    case 1: sleep_now(FALLBACK_S, true, 0, "Round 2: sleeping. Wait 20 s, then touch the screen once to wake it."); break;
    case 2: sleep_now(FALLBACK_S, true, 0, "Round 3: sleeping. Wait 20 s, then press BOOT to wake it."); break;
    case 3:
        if (s_pwr_pins) {
            sleep_now(FALLBACK_S, true, s_pwr_pins, "Round 4: sleeping. Wait 20 s, then press PWR (short) to wake it.");
        }
        break;
    default: break;
    }
    say("Done. Leave it on USB.");
    for (;;) {
        log_power();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void power_diag_start(void)
{
    pmu_init();
    display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    s_label = lv_label_create(scr);
    lv_obj_set_width(s_label, 360);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_label);
    display_unlock();
    xTaskCreatePinnedToCore(diag_task, "pwrdiag", 6144, NULL, 3, NULL, 0);
}
#endif
