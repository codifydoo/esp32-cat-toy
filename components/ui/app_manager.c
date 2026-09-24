#include "app_manager.h"
#include <stdbool.h>
#include <stdint.h>
#include "audio.h"
#include "bedtime.h"
#include "buttons.h"
#include "esp_log.h"
#include "sensors.h"
#include "idle.h"
#include "launcher.h"
#include "sdkconfig.h"
#include "tokens.h"
#include "ui_util.h"

#define SENSOR_POLL_MS 20 /* matches the sensors task's 50 Hz */

static const char *TAG = "app";

static void poll_sensors(lv_timer_t *t);

static const app_t *s_current;
static lv_obj_t *s_screen;  /* the current app's screen; NULL when none is open */
static bool s_pending_home; /* Home arrived before the open fade made s_screen the active screen */
static bool s_started;      /* true once app_manager_start() has run; gates buttons during boot */

#if CONFIG_CAT_HOME_ON_POWER
static lv_timer_t *s_home_timer;    /* pending "act as Home" one-shot, NULL when none is queued */
static uint32_t s_last_power_wake_ms;
#endif

void app_manager_init(void)
{
    launcher_create();
    idle_init();
    bedtime_init();
    lv_timer_create(poll_sensors, SENSOR_POLL_MS, NULL);
}

void app_manager_start(void)
{
    if (!bedtime_take_over()) { /* a start inside the night shows the lock after the boot animation */
        launcher_show();
    }
    s_started = true; /* buttons are ignored until this point -- see handle_button() */
}

bool app_manager_started(void) { return s_started; }

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
     * freed the app's own state. This is the same wait_release + reset pair LVGL itself runs on
     * every pointer indev before deleting an object it is currently pressing -- see
     * obj_indev_reset() in managed_components/lvgl__lvgl/src/core/lv_obj_tree.c. wait_release
     * makes the indev ignore the rest of the current touch; reset(indev, screen) drops this
     * screen as the indev's pressed/last/scrolled object so the next touch starts fresh once
     * the finger lifts (see indev_proc_press()/indev_proc_release() in lv_indev.c). */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            if (lv_indev_get_state(indev) != LV_INDEV_STATE_RELEASED) {
                lv_indev_wait_release(indev);
            }
            lv_indev_reset(indev, screen);
        }
        indev = lv_indev_get_next(indev);
    }

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
        return; /* Home does nothing on the lock (spec §3.12); also a PWR-Home queued before it showed */
    }
    if (idle_wake_if_asleep()) {
        return; /* the press only wakes the screen */
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
        return; /* ignore presses until app_manager_start() has shown the menu (boot animation) */
    }
    lv_display_trigger_activity(NULL); /* a button press counts as activity */
    if (bedtime_showing()) {
        if (id == BUTTON_POWER) {
            bedtime_power(); /* PWR still turns the lock's panel off and on; Home does nothing */
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
            if (s_current && s_current->on_shake && idle_level() == 0) {
                lv_display_trigger_activity(NULL); /* a shake is play (spec §3.10) */
                s_current->on_shake();
            }
            continue;
        }
        const button_id_t id = ev == SENSOR_EV_HOME ? BUTTON_HOME : BUTTON_POWER;
        ESP_LOGI(TAG, "button %s", id == BUTTON_HOME ? "HOME" : "POWER");
        handle_button(id);
    }
    float x, y;
    if (s_current && s_current->on_tilt && idle_level() == 0 && sensors_tilt(&x, &y)) {
        s_current->on_tilt(x, y);
    }
}
