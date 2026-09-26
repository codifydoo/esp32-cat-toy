#include "battery_log.h"
#if CONFIG_CAT_DEV_BATTERY_LOG
#include <stdio.h>
#include <time.h>
#include "batlog.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "pmu.h"
#include "power.h"

#define REG_ADC_EN      0x30 /* bit 0: the battery voltage ADC */
#define REG_VBAT_H      0x34 /* the battery voltage in mV: 6 high bits ... */
#define REG_VBAT_L      0x35 /* ... and 8 low bits */
#define DUMP_EVERY_US   (30 * 1000000LL)
#define LOCK_WAIT_MS    500
#define SLEEP_WAIT_MS   2000 /* the panel is already off: waiting out a flash erase costs nothing */
#define TASK_STACK      4096
#define TASK_PRIORITY   1 /* below everything else: it only samples and prints */
#define NO_PANEL_EVENT  (-1)

static const char *TAG = "batlog";
static SemaphoreHandle_t s_mutex; /* guards everything below except s_frozen and s_panel_event */
static batlog_t s_log;
static nvs_handle_t s_nvs;
static bool s_nvs_ok;
static volatile bool s_frozen; /* the chip is going to sleep: no more records or writes; set without the lock too */
static int64_t s_last_us;      /* uptime of the last record */
static batlog_t s_snapshot;    /* the logging task's copy to print, outside the lock */
static int s_panel_event = NO_PANEL_EVENT; /* BATLOG_DARK or BATLOG_LIT waiting for the task; atomic */

/* The battery voltage in mV, 0 if it cannot be read. Reads high, low, high: two separate reads can tear
 * (0x0EFF then 0x0F00 would read as 0x0E00). */
static uint16_t battery_mv(void)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        uint8_t h1 = 0, lo = 0, h2 = 0;
        if (pmu_read(REG_VBAT_H, &h1) != ESP_OK || pmu_read(REG_VBAT_L, &lo) != ESP_OK ||
            pmu_read(REG_VBAT_H, &h2) != ESP_OK) {
            return 0;
        }
        if (h1 == h2) {
            return (uint16_t)(((h1 & 0x3Fu) << 8) | lo);
        }
    }
    return 0;
}

/* A reading, taken outside the lock (it talks to the PMU). */
static batlog_rec_t sample(batlog_event_t event)
{
    const power_state_t ps = power_state();
    return (batlog_rec_t){
        .time = (uint32_t)time(NULL),
        .mv = battery_mv(),
        .pct = ps.pct < 0 ? BATLOG_NO_PCT : (uint8_t)ps.pct,
        .event = (uint8_t)event,
    };
}

/* Lock held. A failed write keeps the record in RAM; nothing is ever erased. The caller logs the error after
 * releasing the lock (ESP_LOG shares stdout with the dump). */
static esp_err_t save(void)
{
    if (!s_nvs_ok) {
        return ESP_OK;
    }
    esp_err_t err = nvs_set_blob(s_nvs, "ring", &s_log, sizeof(s_log));
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    return err;
}

static void add(batlog_rec_t r)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(LOCK_WAIT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "busy: a %s record was dropped", batlog_event_name(r.event));
        return;
    }
    esp_err_t err = ESP_OK;
    if (!s_frozen) {
        batlog_push(&s_log, r);
        err = save();
        s_last_us = esp_timer_get_time();
    }
    xSemaphoreGive(s_mutex);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving the log failed: %s", esp_err_to_name(err));
    }
}

static bool awake_sample_due(void)
{
    bool due = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(LOCK_WAIT_MS)) == pdTRUE) {
        due = !s_frozen && esp_timer_get_time() - s_last_us >= BATLOG_AWAKE_EVERY_S * 1000000LL;
        xSemaphoreGive(s_mutex);
    }
    return due;
}

/* Prints a copy, so the lock is never held while the console waits for the host (ESP_LOG shares stdout). */
static void dump(void)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(LOCK_WAIT_MS)) != pdTRUE) {
        return;
    }
    s_snapshot = s_log;
    xSemaphoreGive(s_mutex);
    char line[BATLOG_LINE_MAX];
    batlog_rec_t r;
    printf("BATLOG,begin,%u\n", (unsigned)s_snapshot.count);
    for (size_t i = 0; batlog_get(&s_snapshot, i, &r); i++) {
        batlog_format(&r, line, sizeof(line));
        printf("%s\n", line);
    }
    printf("BATLOG,end\n");
    fflush(stdout);
}

static void logging_task(void *arg)
{
    (void)arg;
    bool usb = false; /* a start on USB records usb_in: plugging in never wakes a sleeping toy */
    bool reported_stack = false;
    int64_t last_dump_us = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        const bool now_usb = power_state().usb;
        if (now_usb != usb) {
            usb = now_usb;
            add(sample(usb ? BATLOG_USB_IN : BATLOG_USB_OUT));
        }
        const int panel = __atomic_exchange_n(&s_panel_event, NO_PANEL_EVENT, __ATOMIC_SEQ_CST);
        if (panel != NO_PANEL_EVENT) {
            add(sample((batlog_event_t)panel));
        }
        if (awake_sample_due()) {
            add(sample(BATLOG_AWAKE));
        }
        const int64_t now_us = esp_timer_get_time();
        if (usb && now_us - last_dump_us >= DUMP_EVERY_US) {
            last_dump_us = now_us;
            dump();
            if (!reported_stack) {
                reported_stack = true;
                ESP_LOGI(TAG, "logging task: %u bytes of stack never used", (unsigned)uxTaskGetStackHighWaterMark(NULL));
            }
        }
    }
}

/* A cold start after a crash, a watchdog or a brownout is marked, so a failure mid-run shows in the log. */
static batlog_event_t start_event(void)
{
    switch (deepsleep_cause()) {
    case DEEPSLEEP_BY_PIN: return BATLOG_WAKE_PIN;
    case DEEPSLEEP_BY_TIMER: return BATLOG_WAKE_TIMER;
    default: break;
    }
    switch (esp_reset_reason()) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT: return BATLOG_RESET;
    default: return BATLOG_COLD;
    }
}

void battery_log_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "no memory: no battery log");
        return;
    }
    esp_err_t err = nvs_open("batlog", NVS_READWRITE, &s_nvs);
    s_nvs_ok = err == ESP_OK;
    if (!s_nvs_ok) {
        ESP_LOGE(TAG, "NVS: %s; the log lives in RAM only", esp_err_to_name(err));
    }
    size_t len = sizeof(s_log);
    if (!s_nvs_ok || nvs_get_blob(s_nvs, "ring", &s_log, &len) != ESP_OK || len != sizeof(s_log) ||
        !batlog_valid(&s_log)) {
        batlog_clear(&s_log);
    }
    uint8_t adc = 0;
    if (pmu_read(REG_ADC_EN, &adc) == ESP_OK && !(adc & 1u)) {
        if (pmu_write(REG_ADC_EN, adc | 1u) == ESP_OK) {
            ESP_LOGI(TAG, "battery voltage ADC switched on");
        }
    }
    add(sample(start_event()));
    ESP_LOGW(TAG, "dev build: battery log, %u records; printed every 30 s on USB", (unsigned)s_log.count);
    if (xTaskCreate(logging_task, "batlog", TASK_STACK, NULL, TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "no memory for the logging task");
    }
}

void battery_log_sleep(void)
{
    if (!s_mutex) {
        return;
    }
    const batlog_rec_t r = sample(BATLOG_SLEEP);
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(SLEEP_WAIT_MS)) != pdTRUE) {
        s_frozen = true; /* no new write may start; one already running is cut, and NVS recovers from that */
        ESP_LOGW(TAG, "busy: the sleep was not logged");
        return;
    }
    esp_err_t err = ESP_OK;
    if (!s_frozen) {
        batlog_push(&s_log, r);
        err = save();
    }
    s_frozen = true; /* the task may not start a flash write while the chip goes down */
    xSemaphoreGive(s_mutex);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving the log failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "sleep logged: %d %%, %u mV", r.pct == BATLOG_NO_PCT ? -1 : r.pct, (unsigned)r.mv);
}

void battery_log_panel(bool dark)
{
    __atomic_store_n(&s_panel_event, dark ? BATLOG_DARK : BATLOG_LIT, __ATOMIC_SEQ_CST);
}
#endif
