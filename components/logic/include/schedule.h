#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Bedtime and wake-up rules (spec §3.8, §3.12). Times are local minutes of the day, 0-1439. */
#define SCHEDULE_DAY_MIN       1440
#define SCHEDULE_STARS         5    /* hour stars on the lock */
#define SCHEDULE_SLEEP_STEP    15   /* Bedtime and Wake up move in 15-minute steps */
#define SCHEDULE_NOW_START     720  /* Now's first press while the clock is unset: 12:00 */
#define SCHEDULE_NOW_FAST_MS   2000 /* ± held this long: Now steps by 10 minutes */
#define SCHEDULE_NOW_FAST_STEP 10

#define SCHEDULE_SYNC_BEFORE_MIN 10   /* the daily NTP sync runs this long before wake-up, off the sunrise */
#define SCHEDULE_SYNC_RETRY_S    300  /* while the clock is not valid, a sync is retried this often */
#define SCHEDULE_SYNC_GAP_S      3600 /* the daily sync never follows another attempt closer than this */

#define SCHEDULE_WAKE_LEAD_MIN   SCHEDULE_SYNC_BEFORE_MIN /* the deep-sleep timer wakes the toy this long before
                                                           * wake time, the minute the daily sync is due */
#define SCHEDULE_SLEEP_GUARD_MIN 15 /* no deep sleep in this many minutes before wake time (spec §3.13) */

#define SCHEDULE_SUNRISE_WINDOW_MIN 60 /* the lock's night ends in the sunrise only this long after wake time;
                                        * anywhere else, the clock was set back to before bedtime */

/* Night runs from bed to wake, across midnight when bed > wake; bed == wake means no night. */
bool schedule_is_night(uint16_t now, uint16_t bed, uint16_t wake);
/* Minutes from now forward to target, 0-1439 (0 when they are equal). */
uint16_t schedule_minutes_until(uint16_t now, uint16_t target);
/* now is 0 to SCHEDULE_SUNRISE_WINDOW_MIN - 1 minutes after wake time, across midnight: a lock whose night
 * ends then shows the sunrise. */
bool schedule_sunrise_due(uint16_t now, uint16_t wake);
/* Lit hour stars: the hours left until wake, rounded up, at most SCHEDULE_STARS. */
uint8_t schedule_stars_left(uint16_t now, uint16_t wake);
/* min + delta minutes, wrapped into 0-1439. */
uint16_t schedule_step(uint16_t min, int32_t delta);
/* Writes "HH:MM" and a terminating NUL into out. */
void schedule_format(uint16_t min, char out[6]);
/* The Now row's step while ± has been held for held_ms: 1 minute, then 10 after 2 s. */
uint16_t schedule_now_step(uint32_t held_ms);
/* Whether to start an NTP sync now (spec §3.2). secs_since_attempt: since the last attempt started,
 * UINT32_MAX before the first. */
bool schedule_sync_due(bool valid, uint16_t now, uint16_t wake, uint32_t secs_since_attempt);
/* Deep sleep may start now: never from SCHEDULE_SLEEP_GUARD_MIN minutes before wake time up to and including
 * the wake minute, while the clock is valid and there is a night (spec §3.13). */
bool schedule_sleep_allowed(bool valid, uint16_t now, uint16_t bed, uint16_t wake);
/* The local minute the deep-sleep timer wakes the toy (SCHEDULE_WAKE_LEAD_MIN before wake time), or -1 for no
 * timer: the clock is not valid, or bed == wake (no night). */
int32_t schedule_wake_timer_min(bool valid, uint16_t bed, uint16_t wake);
