#pragma once
#include <stdbool.h>
#include <stdint.h>

/* The battery screens and what a restart shows (spec §3.7, §3.13). Pure: no ESP-IDF. */
#define POWER_LOW_PCT      10 /* the hungry cat shows once at 10 % ... */
#define POWER_LOWER_PCT    5  /* ... and once more at 5 % */
#define POWER_CRITICAL_PCT 2  /* on battery, at or below this the toy deep-sleeps until it is charged */
#define POWER_REARM_PCT    (POWER_LOW_PCT + 5) /* on battery, a reading this high arms both levels again: a charge
                                                * the toy slept through, which USB power could not report (it does
                                                * not wake the chip); the 5-point margin keeps gauge jitter around
                                                * 10 % from re-arming */

typedef struct {
    bool armed_low, armed_lower; /* the 10 % and 5 % screens have not shown since USB power was connected or
                                  * the battery read POWER_REARM_PCT or more */
    bool pending;                /* a low-battery screen is due and has not shown yet */
} battery_rules_t;

typedef enum { BATTERY_OK, BATTERY_CRITICAL } battery_verdict_t;

typedef enum {
    WOKE_COLD,  /* power-on or a reset */
    WOKE_TOUCH, /* from deep sleep by a touch or BOOT (or PWR, where wired) */
    WOKE_TIMER, /* from deep sleep by the timer before wake time */
} woke_t;

typedef enum {
    WAKE_BOOT,      /* the boot animation, then the menu, the lock or the first-run picker */
    WAKE_LOCK_LIT,  /* the lock at once, lit as after a touch */
    WAKE_LOCK_DARK, /* the lock at once with its panel off: the sunrise follows at wake time */
    WAKE_SUNRISE,   /* the sunrise at once: the timer fired after wake time */
    WAKE_HUNGRY,    /* the battery is still empty: the hungry cat for 4 s, then deep sleep again */
} wake_plan_t;

void battery_rules_init(battery_rules_t *r); /* both levels armed, nothing pending */
/* One reading: pct 0-100, or anything else when the gauge is unavailable, which never shows a screen or
 * sleeps (spec §4.7). Crossing 10 % or 5 % on battery makes one screen pending (one only, even when a jump
 * crosses both). USB power, or a reading of POWER_REARM_PCT or more on battery (a charge the toy slept through),
 * arms both levels again and drops a pending screen. */
battery_verdict_t battery_rules_update(battery_rules_t *r, int pct, bool usb);
/* True once per pending screen, which the caller then shows. Call only when it can show. */
bool battery_rules_take_pending(battery_rules_t *r);
/* On battery at POWER_CRITICAL_PCT or less, with a valid reading. */
bool battery_critical(int pct, bool usb);
/* battery_sleep: the toy went to sleep for the battery; critical: battery_critical() now. */
wake_plan_t power_wake_plan(woke_t woke, bool night, bool battery_sleep, bool critical);
