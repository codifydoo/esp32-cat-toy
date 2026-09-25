#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* The time (spec §3.2). The 1.75C has no RTC chip: the ESP32-S3 keeps the time through restarts, not
 * through a power loss. */
#define CLOCK_TZ              "CET-1CEST,M3.5.0,M10.5.0/3" /* Europe/Zagreb */
#define CLOCK_VALID_FROM      1767225600                   /* 2026-01-01 00:00 UTC */
#define CLOCK_NTP_SERVER      "pool.ntp.org"
#define CLOCK_SYNC_TIMEOUT_MS 10000 /* Wi-Fi connect and NTP together */
#define CLOCK_SNTP_RESEND_MS  2000  /* an unanswered NTP request is sent again after this (lwIP waits 15 s) */

/* Sets the time zone and, when Wi-Fi is configured, the network stack. Call after settings_init()
 * (Wi-Fi keeps its calibration in NVS). */
esp_err_t clock_init(void);
/* The clock reads 2026 or later: only an NTP sync or a manual set since power-on makes this true. */
bool clock_valid(void);
/* Local minute of the day, 0-1439. Meaningless while !clock_valid(). */
uint16_t clock_now_min(void);
/* Sets the local time of day to min (seconds 0). Keeps today's date, or uses 2026-01-02 when the clock
 * has no date yet: that date stays in 2026 in UTC even at 00:00, so the time counts as valid. */
void clock_set_now_min(uint16_t min);
/* Starts one NTP sync in the background when Wi-Fi is configured and no sync is running. Any task. */
void clock_sync_start(void);
/* Seconds since the last sync attempt started (esp_timer_get_time(), unaffected by clock changes);
 * UINT32_MAX before the first. LVGL task. */
uint32_t clock_secs_since_attempt(void);
