#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Deep sleep on battery (spec §3.7, §3.13). */
typedef enum {
    DEEPSLEEP_COLD,     /* power-on or a reset */
    DEEPSLEEP_BY_PIN,   /* a touch (GPIO11), BOOT (GPIO0), or PWR where wired */
    DEEPSLEEP_BY_TIMER, /* the timer set by deepsleep_start() */
} deepsleep_cause_t;

/* Call first in app_main: reads why the chip started, and releases the speaker amplifier's sleep hold. */
void deepsleep_init(void);
deepsleep_cause_t deepsleep_cause(void);
/* The deep sleep this start woke from was for the battery (deepsleep_start(.., true)). */
bool deepsleep_was_for_battery(void);
/* Stops all sound, puts the panel to sleep, holds the speaker amplifier off and deep-sleeps. Never returns:
 * the chip restarts on a touch, BOOT (PWR where wired), or after timer_s seconds when timer_s > 0. LVGL task,
 * with the LVGL lock held. */
void deepsleep_start(uint32_t timer_s, bool for_battery);
