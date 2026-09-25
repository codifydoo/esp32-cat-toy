#include "power_watch.h"
#include "app_manager.h"
#include "bedtime.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "first_run.h"
#include "idle.h"
#include "overlays.h"
#include "parent.h"
#include "power.h"
#include "power_rules.h"

#define POWER_WATCH_MS 1000

static const char *TAG = "power_watch";
static battery_rules_t s_rules;

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
            overlay_show(OVERLAY_HUNGRY, sleep_for_battery); /* from anywhere: the clock must survive */
        }
        return;
    }
    if (can_show() && idle_level() == 0 && battery_rules_take_pending(&s_rules)) {
        overlay_show(OVERLAY_HUNGRY, NULL);
    }
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
