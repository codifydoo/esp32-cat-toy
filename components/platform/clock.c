#include "clock.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define SYNC_STACK   6144
#define BIT_GOT_IP   BIT0
#define BIT_NO_WIFI  BIT1

static const char *TAG = "clock";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_running;          /* a sync task exists; under s_mux */
static bool s_net_ready;        /* Wi-Fi is configured and the network stack is up */
static EventGroupHandle_t s_events;
static int64_t s_last_sync_us;  /* esp_timer time of the last successful sync, 0 before */

static void log_time(const char *what)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    ESP_LOGI(TAG, "%s: %s%s", what, buf, clock_valid() ? "" : " (not valid)");
}

esp_err_t clock_init(void)
{
    setenv("TZ", CLOCK_TZ, 1);
    tzset();
    log_time("time at boot");
    if (strlen(CONFIG_CAT_WIFI_SSID) == 0) {
        ESP_LOGI(TAG, "no Wi-Fi configured: the parent sets the clock by hand");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    const esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop");
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "event group");
    s_net_ready = true;
    return ESP_OK;
}

bool clock_valid(void) { return time(NULL) >= CLOCK_VALID_FROM; }

uint16_t clock_now_min(void)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return (uint16_t)(tm.tm_hour * 60 + tm.tm_min);
}

void clock_set_now_min(uint16_t min)
{
    const time_t now = time(NULL);
    struct tm tm;
    if (clock_valid()) {
        localtime_r(&now, &tm);
    } else {
        tm = (struct tm){.tm_year = 2026 - 1900, .tm_mon = 0, .tm_mday = 2};
    }
    tm.tm_hour = (min % 1440) / 60;
    tm.tm_min = min % 60;
    tm.tm_sec = 0;
    tm.tm_isdst = -1; /* mktime picks CET or CEST for that date */
    const struct timeval tv = {.tv_sec = mktime(&tm)};
    settimeofday(&tv, NULL);
    log_time("set by hand");
}

static void on_net_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_events, BIT_NO_WIFI); /* no network or a wrong password: give up this time */
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, BIT_GOT_IP);
    }
}

static TickType_t ticks_left(TickType_t deadline)
{
    const TickType_t now = xTaskGetTickCount();
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0;
}

/* Wi-Fi on, one NTP exchange, then Wi-Fi stopped and released. On success *correction_us is how far the
 * clock moved beyond the real time that passed: the drift since the last sync (spec §3.2). */
static bool sync_once(int64_t *correction_us)
{
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CLOCK_SYNC_TIMEOUT_MS);
    xEventGroupClearBits(s_events, BIT_GOT_IP | BIT_NO_WIFI);
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (!netif || esp_wifi_init(&init) != ESP_OK) {
        if (netif) {
            esp_netif_destroy_default_wifi(netif);
        }
        return false;
    }
    esp_event_handler_instance_t on_wifi = NULL, on_ip = NULL;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_net_event, NULL, &on_wifi);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_net_event, NULL, &on_ip);
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, CONFIG_CAT_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, CONFIG_CAT_WIFI_PASSWORD, sizeof(cfg.sta.password));
    bool ok = false;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK && esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK &&
        esp_wifi_set_config(WIFI_IF_STA, &cfg) == ESP_OK && esp_wifi_start() == ESP_OK) {
        const EventBits_t bits =
            xEventGroupWaitBits(s_events, BIT_GOT_IP | BIT_NO_WIFI, pdFALSE, pdFALSE, ticks_left(deadline));
        if (bits & BIT_GOT_IP) {
            esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(CLOCK_NTP_SERVER);
            if (esp_netif_sntp_init(&sntp) == ESP_OK) {
                struct timeval t0, t1;
                gettimeofday(&t0, NULL);
                const int64_t m0 = esp_timer_get_time();
                ok = esp_netif_sntp_sync_wait(ticks_left(deadline)) == ESP_OK;
                if (ok) {
                    gettimeofday(&t1, NULL);
                    const int64_t m1 = esp_timer_get_time();
                    *correction_us = ((int64_t)(t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec)) -
                                     (m1 - m0);
                }
                esp_netif_sntp_deinit();
            }
        }
        esp_wifi_stop();
    }
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi);
    esp_wifi_deinit();
    esp_netif_destroy_default_wifi(netif);
    return ok;
}

static void sync_task(void *arg)
{
    (void)arg;
    const size_t int0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t ps0 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int64_t correction_us = 0;
    if (sync_once(&correction_us)) {
        const int64_t now_us = esp_timer_get_time();
        ESP_LOGI(TAG, "NTP: corrected by %+lld ms, %lld s after the previous sync", (long long)(correction_us / 1000),
                 s_last_sync_us ? (long long)((now_us - s_last_sync_us) / 1000000) : -1LL);
        s_last_sync_us = now_us;
        log_time("time now");
    } else {
        ESP_LOGW(TAG, "NTP sync failed; keeping the current time");
    }
    const size_t int1 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t ps1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "after the sync: free internal %u (%+ld), psram %u (%+ld)", (unsigned)int1, (long)int1 - (long)int0,
             (unsigned)ps1, (long)ps1 - (long)ps0);
    portENTER_CRITICAL(&s_mux);
    s_running = false;
    portEXIT_CRITICAL(&s_mux);
    vTaskDelete(NULL);
}

void clock_sync_start(void)
{
    if (!s_net_ready) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    const bool start = !s_running;
    s_running = true;
    portEXIT_CRITICAL(&s_mux);
    if (!start) {
        return;
    }
    if (xTaskCreatePinnedToCore(sync_task, "ntp", SYNC_STACK, NULL, 2, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "no memory for the NTP task");
        portENTER_CRITICAL(&s_mux);
        s_running = false;
        portEXIT_CRITICAL(&s_mux);
    }
}
