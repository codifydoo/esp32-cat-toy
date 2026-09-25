#include "power_watch.h"
#include "app_manager.h"
#include "bedtime.h"
#include "clock.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "first_run.h"
#include "idle.h"
#include "overlays.h"
#include "parent.h"
#include "power.h"
#include "power_rules.h"
#include "schedule.h"
#include "sdkconfig.h"
#include "settings.h"
#include "tokens.h"

#define POWER_WATCH_MS 1000

#if CONFIG_CAT_DEV_FAST_SLEEP
#define SLEEP_AFTER_DARK_MS 20000 /* the power and drain dev builds */
#else
#define SLEEP_AFTER_DARK_MS DEEP_SLEEP_AFTER_DARK_MS
#endif

static const char *TAG = "power_watch";
static battery_rules_t s_rules;
static bool s_dark;
static uint32_t s_dark_since;

/* Where the battery screens may show (spec §3.13): over the menu or an open app, never over the boot animation,
 * the cat picker, the parent area, the lock or the sunrise. */
static bool can_show(void)
{
    return app_manager_started() && !overlay_showing() && !first_run_showing() && !bedtime_showing() &&
           app_manager_current() != &PARENT_APP;
}

/* The panel is off: the menu's idle off or PWR, or the lock's dark panel. */
static bool panel_dark(void) { return bedtime_showing() ? !bedtime_panel_lit() : idle_level() == 3; }

static void sleep_for_battery(void) { deepsleep_start(0, true); }

/* Deep sleep on battery, SLEEP_AFTER_DARK_MS after the panel went off (spec §3.13): never on USB power, during a
 * battery screen or an NTP sync, or in the last quarter hour before wake time. The timer wakes the toy before
 * wake time while the clock is valid and there is a night. */
static void sleep_check(bool usb)
{
    if (!panel_dark()) {
        s_dark = false;
        return;
    }
    if (!s_dark) {
        s_dark = true;
        s_dark_since = lv_tick_get();
        return;
    }
    if (lv_tick_elaps(s_dark_since) < SLEEP_AFTER_DARK_MS || usb || overlay_showing() || clock_sync_running()) {
        return;
    }
    const bool valid = clock_valid();
    const uint16_t now = clock_now_min(), bed = settings_bedtime(), wake = settings_wake();
    if (!schedule_sleep_allowed(valid, now, bed, wake)) {
        return;
    }
    const int32_t at = schedule_wake_timer_min(valid, bed, wake);
    deepsleep_start(at < 0 ? 0 : clock_secs_until((uint16_t)at), false);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (!app_manager_started()) {
        return; /* the boot animation */
    }
    const power_state_t ps = power_state();
    if (battery_rules_update(&s_rules, ps.pct, ps.usb) == BATTERY_CRITICAL) {
        if (overlay_showing()) {
            return; /* a screen is up: the next tick takes over once it has gone (or its done sleeps) */
        }
        ESP_LOGW(TAG, "battery %d %%: sleeping until it is charged", ps.pct);
        if (panel_dark()) {
            sleep_for_battery();
        } else {
            idle_wake_if_asleep(); /* a dim menu lights up: she sees the hungry cat before the toy sleeps */
            overlay_show(OVERLAY_HUNGRY, sleep_for_battery); /* from anywhere: the clock must survive */
        }
        return;
    }
    if (can_show() && idle_level() == 0 && battery_rules_take_pending(&s_rules)) {
        overlay_show(OVERLAY_HUNGRY, NULL);
    }
    sleep_check(ps.usb);
}

void power_watch_init(void)
{
    battery_rules_init(&s_rules);
    lv_timer_create(tick, POWER_WATCH_MS, NULL);
}

void power_watch_usb_in(void)
{
    if (power_state().pct < 0 || !can_show()) {
        return; /* no gauge, or somewhere it never shows, and it is not shown later (spec §3.13) */
    }
    idle_wake_if_asleep(); /* connecting USB counts as a touch: a dim or dark menu lights up first */
    overlay_show(OVERLAY_CHARGING, NULL);
}
