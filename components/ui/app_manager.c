#include "app_manager.h"
#include <stdbool.h>
#include <stdint.h>
#include "audio.h"
#include "bedtime.h"
#include "boot.h"
#include "buttons.h"
#include "deepsleep.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sensors.h"
#include "first_run.h"
#include "idle.h"
#include "launcher.h"
#include "overlays.h"
#include "parent.h"
#include "power.h"
#include "power_rules.h"
#include "power_watch.h"
#include "sdkconfig.h"
#include "tokens.h"
#include "ui_util.h"

#define SENSOR_POLL_MS 20 /* matches the sensors task's 50 Hz */

static const char *TAG = "app";

static void poll_sensors(lv_timer_t *t);

static const app_t *s_current;
static lv_obj_t *s_screen;  /* the current app's screen; NULL when none is open */
static bool s_pending_home; /* Home arrived before the open fade made s_screen the active screen */
static bool s_started;      /* true once the start's first screen shows (app_manager_start(), or a deep-sleep
                              * wake's lock/sunrise in app_manager_boot()); gates buttons during boot */

#if CONFIG_CAT_HOME_ON_POWER
static lv_timer_t *s_home_timer;    /* pending "act as Home" one-shot, NULL when none is queued */
static uint32_t s_last_power_wake_ms;
#endif

void app_manager_init(void)
{
    launcher_create();
    idle_init();
    bedtime_init();
    power_watch_init();
    lv_timer_create(poll_sensors, SENSOR_POLL_MS, NULL);
}

void app_manager_start(void)
{
    if (first_run_due()) {
        first_run_show(); /* the picker; its end shows the lock or the menu */
    } else if (!bedtime_take_over()) { /* a start inside the night shows the lock after the boot animation */
        launcher_show();
    }
    s_started = true; /* buttons are ignored until this point -- see handle_button() */
}

bool app_manager_started(void) { return s_started; }

static woke_t woke(void)
{
    switch (deepsleep_cause()) {
    case DEEPSLEEP_BY_PIN: return WOKE_TOUCH;
    case DEEPSLEEP_BY_TIMER: return WOKE_TIMER;
    default: return WOKE_COLD;
    }
}

/* The hungry cat of a WAKE_HUNGRY start has ended. With USB power plugged in meanwhile, the start goes on as a
 * cold one does; no NTP here: the daily sync and the invalid-clock retry cover it. */
static void sleep_again(void)
{
    if (power_on_usb()) {
        ESP_LOGI(TAG, "USB power: starting instead of sleeping");
        display_set_brightness(BRIGHTNESS_FULL); /* as a cold start: the night's 35 % would stay on the menu */
        boot_run(app_manager_start);
        return;
    }
    deepsleep_start(0, true);
}

bool app_manager_boot(void)
{
    const power_state_t ps = power_state(); /* power_init() took a first reading */
    wake_plan_t plan = power_wake_plan(woke(), bedtime_is_night(), deepsleep_was_for_battery(),
                                        battery_critical(ps.pct, ps.usb));
    if (first_run_due() && plan != WAKE_HUNGRY) {
        /* The picker isn't done: a toy that slept on it shows it again, as a cold start at night does. */
        plan = WAKE_BOOT;
    }
    static const char *const NAMES[] = {"boot animation", "lock, lit", "lock, dark", "sunrise", "hungry cat"};
    ESP_LOGI(TAG, "start: %s (free psram %u)", NAMES[plan], (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    switch (plan) {
    case WAKE_LOCK_LIT:
    case WAKE_LOCK_DARK:
        bedtime_wake(plan == WAKE_LOCK_DARK);
        s_started = true;
        break;
    case WAKE_SUNRISE:
        bedtime_sunrise_now();
        s_started = true;
        break;
    case WAKE_HUNGRY:
        if (bedtime_is_night()) {
            display_set_brightness(BEDTIME_BRIGHTNESS); /* at night, as bright as the lock */
        }
        overlay_show(OVERLAY_HUNGRY, sleep_again); /* the buttons stay ignored: s_started is false */
        break;
    default: boot_run(app_manager_start); break;
    }
    /* The hungry cat sleeps again, and any other start at 2 % soon sleeps for the battery (power_watch): no NTP. */
    return plan != WAKE_HUNGRY && !battery_critical(ps.pct, ps.usb);
}

#if CONFIG_CAT_HOME_ON_POWER
static void cancel_pending_home(void)
{
    if (s_home_timer) {
        lv_timer_delete(s_home_timer);
        s_home_timer = NULL;
    }
}
#endif

static void close_current_app(void)
{
    const app_t *app = s_current;
    lv_obj_t *screen = s_screen;
    s_current = NULL;
    s_screen = NULL;
    /* A close reached here by any path (direct, or the deferred one below) fully settles this
     * open/close cycle, so any stale request from earlier in it must not leak into the next
     * one -- see app_manager_open()'s comment on why this can otherwise go stale. */
    s_pending_home = false;
#if CONFIG_CAT_HOME_ON_POWER
    /* A pending PWR-Home queued just before this close (from any path, including the app's own
     * idle timeout) must not fire 400 ms later against whatever the menu/app state becomes next. */
    cancel_pending_home();
#endif

    /* Cut pointer input to the app screen before destroy(): a press already in flight on it can
     * otherwise still reach the app's event callbacks (e.g. a tap handler) after destroy() has
     * freed the app's own state -- see ui_cut_touch()'s comment in ui_util.c for how. */
    ui_cut_touch(screen);

    app->destroy();
    audio_stop_all();
    if (bedtime_take_over()) {
        return; /* night: the lock instead of the menu, without the Home mew (spec §3.12) */
    }
    if (idle_level() != 3) {
        /* Screen is dark (e.g. Power turned it off, then this app's own idle timeout closed it):
         * don't mew in the dark. */
        audio_play(SND_HOME);
    }
    launcher_show(); /* fades the menu in and deletes the app screen */
}

static void deferred_home(void *user_data)
{
    (void)user_data;
    if (!s_current) {
        return; /* a direct-path Home (see app_manager_home()) already closed it */
    }
    close_current_app();
}

/* LV_EVENT_SCREEN_LOADED fires once the app screen actually becomes the active screen: for a
 * screen-load animation that happens from inside scr_anim_completed(), itself called from the
 * anim-refresher timer on the NEXT tick after lv_screen_load_anim() returns (its start_cb,
 * scr_load_anim_start(), is what flips lv_display_t.act_scr, and that too only runs on that next
 * tick) -- see managed_components/lvgl__lvgl/src/display/lv_display.c. The LVGL lock is released
 * between worker-task ticks (see lvgl_worker() in esp_lv_adapter.c), so Home can arrive in that
 * gap while lv_screen_active() still reports the menu screen. If it did, finish the open first
 * and only then close, on a fresh lv_async_call() rather than re-entering screen loading from
 * inside this screen-load event. */
static void on_screen_loaded(lv_event_t *e)
{
    (void)e;
    if (s_pending_home) {
        s_pending_home = false;
        lv_async_call(deferred_home, NULL);
    }
}

void app_manager_open(const app_t *app)
{
    if (s_current || !app) {
        return;
    }
    if (app != &PARENT_APP && bedtime_showing()) {
        return; /* the menu's open zoom can end after bedtime took over: no app at night (spec §3.12) */
    }
#if CONFIG_CAT_HOME_ON_POWER
    /* Opening an app is itself fresh activity; a PWR-Home queued just before this touch must not
     * fire against the app that is about to open. */
    cancel_pending_home();
#endif
    s_current = app;
    /* A Home from the previous open/close cycle can be left pending here: if a second Home
     * landed once that cycle's app screen had genuinely become active, it took the direct path
     * in app_manager_home() below, whose launcher_show() -> lv_screen_load_anim() deletes that
     * app screen's still-pending load anim via lv_anim_delete() (lv_display.c) without ever
     * running its completed_cb (remove_anim() only calls deleted_cb, never completed_cb --
     * managed_components/lvgl__lvgl/src/misc/lv_anim.c:816-822), so scr_anim_completed() and
     * LV_EVENT_SCREEN_LOADED never fired for it and on_screen_loaded() never got to clear the
     * flag itself. Start this new app with a clean slate regardless. */
    s_pending_home = false;
    s_screen = ui_screen();
    lv_obj_add_event_cb(s_screen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    sensors_tilt_recenter(); /* the grip she opens it with is level (spec §3.10) */
    app->create(s_screen);
    lv_screen_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_OPEN_FADE_MS, 0, false);
}

void app_manager_home(void)
{
    if (!s_current) {
        return;
    }
    if (lv_screen_active() != s_screen) {
        /* The open fade hasn't switched the active screen yet; closing now would leave the app
         * screen loaded but orphaned once it does. Finish opening first (see on_screen_loaded()). */
        s_pending_home = true;
        return;
    }
    close_current_app();
}

const app_t *app_manager_current(void) { return s_current; }

/* Shared "act as Home" logic: wakes a dim/sleeping/off screen instead of acting, otherwise closes
 * the current app or re-announces the menu. Used directly by BOOT. Without CAT_HOME_ON_POWER, PWR
 * does not reach here -- handle_button() diverts it straight to idle_power_toggle(). With
 * CAT_HOME_ON_POWER, PWR reaches it only via the deferred single-press timer (pending_home_cb). */
static void do_home(void)
{
#if CONFIG_CAT_HOME_ON_POWER
    cancel_pending_home(); /* a Home about to run makes any pending PWR-Home stale */
#endif
    if (bedtime_showing()) {
        bedtime_home(); /* does nothing on the lock, ends the sunrise; also drops a PWR-Home queued before it showed */
        return;
    }
    if (idle_wake_if_asleep()) {
        return; /* the press only wakes the screen */
    }
    if (first_run_showing()) {
        first_run_skip(); /* Home works everywhere but on the lock (spec §3.6, §3.13) */
        return;
    }
    if (s_current) {
        app_manager_home();
    } else {
        launcher_home_pressed();
    }
}

#if CONFIG_CAT_HOME_ON_POWER
static void pending_home_cb(lv_timer_t *t)
{
    (void)t;
    s_home_timer = NULL;
    do_home();
}
#endif

/* Runs on the LVGL task (poll_sensors below). */
static void handle_button(button_id_t id)
{
    if (!s_started) {
        return; /* ignore presses until the start's first screen has shown (the boot animation, or a deep-sleep
                 * wake's lock or sunrise) */
    }
    lv_display_trigger_activity(NULL); /* a button press counts as activity */
    if (overlay_showing()) {
        overlay_close(); /* Home and PWR work as always; the battery screen ends first (spec §3.13) */
    }
    if (bedtime_showing()) {
        if (id == BUTTON_POWER) {
            bedtime_power(); /* PWR still turns the lock's panel off and on */
        } else {
            bedtime_home(); /* Home does nothing on the lock; it ends the sunrise */
        }
        return;
    }
#if CONFIG_CAT_HOME_ON_POWER
    if (id == BUTTON_POWER) {
        if (idle_wake_if_asleep()) {
            s_last_power_wake_ms = lv_tick_get();
            return; /* this press only woke the screen */
        }
        if (lv_tick_elaps(s_last_power_wake_ms) < 400) {
            return; /* a press right after a PWR-triggered wake is ignored */
        }
        if (s_home_timer) {
            /* second awake press within the window: cancel the pending Home, turn off instead */
            cancel_pending_home();
            idle_power_toggle();
            return;
        }
        /* first awake press: wait out the double-press window before acting as Home */
        s_home_timer = lv_timer_create(pending_home_cb, 400, NULL);
        lv_timer_set_repeat_count(s_home_timer, 1);
        return;
    }
#else
    if (id == BUTTON_POWER) {
        idle_power_toggle();
        return;
    }
#endif
    do_home();
}

/* Runs on the LVGL task: drains the sensors task's queue and passes the tilt to the open app. */
static void poll_sensors(lv_timer_t *t)
{
    (void)t;
    sensor_ev_t ev;
    while (sensors_next(&ev)) {
        if (ev == SENSOR_EV_SHAKE) {
            if (s_current && s_current->on_shake && idle_level() == 0 && !overlay_showing()) {
                lv_display_trigger_activity(NULL); /* a shake is play (spec §3.10) */
                s_current->on_shake();
            }
            continue;
        }
        if (ev == SENSOR_EV_USB_IN) {
            power_watch_usb_in();
            continue;
        }
        const button_id_t id = ev == SENSOR_EV_HOME ? BUTTON_HOME : BUTTON_POWER;
        ESP_LOGI(TAG, "button %s", id == BUTTON_HOME ? "HOME" : "POWER");
        handle_button(id);
    }
    float x, y;
    if (s_current && s_current->on_tilt && idle_level() == 0 && !overlay_showing() && sensors_tilt(&x, &y)) {
        s_current->on_tilt(x, y);
    }
}
