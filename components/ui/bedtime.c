#include "bedtime.h"
#include <stdint.h>
#include "app_manager.h"
#include "audio.h"
#include "cat.h"
#include "clock.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "hold_ring.h"
#include "idle.h"
#include "launcher.h"
#include "parent.h"
#include "power.h"
#include "scenes.h"
#include "schedule.h"
#include "sdkconfig.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"

_Static_assert(HOUR_STARS == SCHEDULE_STARS, "one hour star for each star the schedule counts");

typedef enum { BT_DAY, BT_LOCK, BT_SUNRISE } bt_state_t;

static const char *TAG = "bedtime";
static bt_state_t s_state;
static lv_obj_t *s_scr;      /* the lock's screen, reused by the sunrise; NULL in the day */
static night_scene_t s_night;
static day_scene_t s_day;    /* the rays read their points from here */
/* The lock's panel (spec §3.12): lit all night on USB unless PWR turned it off; otherwise lit until
 * s_on_until (30 s after the lock shows, 5 s after a touch). */
static bool s_lit, s_user_off, s_usb;
static uint32_t s_on_until;
static bool s_whispered; /* s_last_whisper is set */
static bool s_parent_hold; /* a two-finger parent hold is in progress: suppress the whisper on release */
static uint32_t s_last_whisper, s_last_shift;
static lv_timer_t *s_shh_timer, *s_menu_timer;
static bool s_sun_up;   /* the sun has risen: a tap goes to the menu */
static int s_fade_var;  /* identifies the sunrise's brightness anim (lv_anim needs a non-NULL var) */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void delete_timer(lv_timer_t **t)
{
    if (*t) {
        lv_timer_delete(*t);
        *t = NULL;
    }
}

static bool night_now(void)
{
#if CONFIG_CAT_DEV_SOAK
    return false; /* the soak opens and closes apps around the clock; the lock would take them over */
#else
    return clock_valid() && schedule_is_night(clock_now_min(), settings_bedtime(), settings_wake());
#endif
}

static void log_heap(const char *what)
{
    ESP_LOGI(TAG, "%s: free internal %u, psram %u", what, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* ---- the lock's panel ---- */

static void set_lit(bool lit)
{
    if (lit == s_lit) {
        return;
    }
    s_lit = lit;
    ui_set_hidden(s_night.root, !lit); /* hidden, the twinkles stop drawing */
    display_set_brightness(lit ? BEDTIME_BRIGHTNESS : 0);
}

static void update_panel(void)
{
    set_lit((s_usb && !s_user_off) || (int32_t)(s_on_until - lv_tick_get()) > 0);
}

/* Keeps the scene lit for at least ms from now. */
static void keep_lit(uint32_t ms)
{
    const uint32_t until = lv_tick_get() + ms;
    if ((int32_t)(until - s_on_until) > 0) {
        s_on_until = until;
    }
}

/* ---- the lock ---- */

static void shh_done(lv_timer_t *t)
{
    (void)t;
    s_shh_timer = NULL; /* one-shot timers delete themselves */
    if (s_state == BT_LOCK) {
        cat_set_state(s_night.cat, CAT_SLEEPING);
    }
}

/* A touch shows the scene for 5 s. PRESSED only lights it; the whisper "Pssst… spavaj", at most once
 * every 5 s, moves to RELEASED so a parent's two-finger hold into the parent area never whispers --
 * s_parent_hold makes that independent of the hold ring's own lv_indev_wait_release(). */
static void lock_pressed(lv_event_t *e)
{
    (void)e;
    if (s_state != BT_LOCK) {
        return;
    }
    s_parent_hold = false;
    keep_lit(BEDTIME_TOUCH_ON_MS);
    update_panel();
}

static void lock_released(lv_event_t *e)
{
    (void)e;
    if (s_state != BT_LOCK || s_parent_hold) {
        return;
    }
    if (s_whispered && lv_tick_elaps(s_last_whisper) < BEDTIME_WHISPER_GAP_MS) {
        return;
    }
    s_whispered = true;
    s_last_whisper = lv_tick_get();
    audio_play_quiet(SND_WHISPER, BEDTIME_WHISPER_GAIN);
    cat_set_state(s_night.cat, CAT_SHH);
    delete_timer(&s_shh_timer);
    s_shh_timer = one_shot(shh_done, BEDTIME_SHH_MS);
}

static void hold_holding(void)
{
    s_parent_hold = true;
    keep_lit(BEDTIME_TOUCH_ON_MS); /* the ring stays visible while the parent holds */
    update_panel();
}

static void hold_done(void)
{
    display_set_brightness(BRIGHTNESS_FULL); /* the parent area is never dimmed */
    lv_display_trigger_activity(NULL);
    app_manager_open(&PARENT_APP); /* leaving it comes back through bedtime_take_over() */
}

static void build_lock(void)
{
    s_scr = ui_screen();
    scene_night_create(&s_night, s_scr);
    lv_obj_add_event_cb(s_scr, lock_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_scr, lock_released, LV_EVENT_RELEASED, NULL);
    static const hold_ring_cb_t HOLD = {.holding = hold_holding, .done = hold_done};
    hold_ring_attach(s_scr, BEDTIME_RING_CX, BEDTIME_RING_CY, &HOLD);
}

/* Shows the lock (built once per night) and lights it: 30 s on battery, all night on USB. */
static void show_lock(bool delete_previous)
{
    if (!s_scr) {
        build_lock();
    }
    s_state = BT_LOCK;
    scene_night_set_stars(&s_night, schedule_stars_left(clock_now_min(), settings_wake()));
    idle_hand_over(); /* the lock runs its own panel rules */
    s_user_off = false;
    s_usb = power_on_usb();
    s_on_until = lv_tick_get() + BEDTIME_ON_MS;
    s_lit = false;
    set_lit(true);
    s_last_shift = lv_tick_get();
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_HOME_MS, 0, delete_previous);
    ESP_LOGI(TAG, "lock (USB power: %s)", s_usb ? "yes" : "no");
    log_heap("lock shown");
}

/* The night is over without a sunrise (the parent moved the times). The caller shows the menu, and that
 * load deletes the screen that is active then. */
static void drop_lock(void)
{
    delete_timer(&s_shh_timer);
    if (s_scr && s_scr != lv_screen_active()) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_night = (night_scene_t){0}; /* its objects are gone with the screen: don't keep pointers to them */
    s_state = BT_DAY;
    log_heap("lock dropped");
}

static void lock_tick(uint16_t now)
{
    scene_night_set_stars(&s_night, schedule_stars_left(now, settings_wake()));
    const bool usb = power_on_usb();
    if (usb != s_usb) {
        ESP_LOGI(TAG, "USB power: %s", usb ? "yes" : "no");
        if (!usb) {
            keep_lit(BEDTIME_ON_MS); /* unplugged: 30 s, as on battery */
        }
        s_usb = usb;
    }
    update_panel();
    if (lv_tick_elaps(s_last_shift) >= BURN_SHIFT_MS) {
        s_last_shift = lv_tick_get();
        const int32_t span = 2 * BURN_SHIFT_PX + 1;
        lv_obj_set_pos(s_night.root, (int32_t)(esp_random() % span) - BURN_SHIFT_PX,
                       (int32_t)(esp_random() % span) - BURN_SHIFT_PX);
    }
}

/* ---- the sunrise ---- */

static void set_brightness(void *var, int32_t v)
{
    (void)var;
    display_set_brightness((uint8_t)v);
}

static void finish_sunrise(void)
{
    delete_timer(&s_menu_timer);
    lv_anim_delete(&s_fade_var, set_brightness);
    scene_day_stop(&s_day); /* stops the turning rays and the rise before the menu fades in */
    display_set_brightness(BRIGHTNESS_FULL);
    s_state = BT_DAY;
    s_scr = NULL; /* launcher_show() deletes the screen once the menu has faded in */
    lv_display_trigger_activity(NULL); /* the quiet night must not dim the menu at once */
    launcher_show();
    log_heap("sunrise over");
}

static void menu_later(lv_timer_t *t)
{
    (void)t;
    s_menu_timer = NULL; /* one-shot timers delete themselves */
    if (s_state == BT_SUNRISE) {
        finish_sunrise();
    }
}

static void sun_up(lv_anim_t *a)
{
    (void)a;
    if (s_state != BT_SUNRISE) {
        return;
    }
    s_sun_up = true;
    audio_play(SND_WAKE); /* the rising chime, a happy meow and "Dobro jutro!" */
    s_menu_timer = one_shot(menu_later, SUNRISE_MENU_MS);
}

static void sunrise_clicked(lv_event_t *e)
{
    (void)e;
    if (s_state == BT_SUNRISE && s_sun_up) {
        finish_sunrise();
    }
}

static void start_sunrise(void)
{
    delete_timer(&s_shh_timer);
    s_state = BT_SUNRISE;
    s_sun_up = false;
    lv_obj_clean(s_scr); /* the night scene, and the hold ring with its timer */
    s_night = (night_scene_t){0};
    scene_day_create(&s_day, s_scr);
    lv_obj_add_event_cb(s_scr, sunrise_clicked, LV_EVENT_CLICKED, NULL);
    display_set_brightness(SUNRISE_FROM_PCT);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &s_fade_var);
    lv_anim_set_exec_cb(&a, set_brightness);
    lv_anim_set_values(&a, SUNRISE_FROM_PCT, BRIGHTNESS_FULL);
    lv_anim_set_duration(&a, DAY_RISE_MS);
    lv_anim_set_completed_cb(&a, sun_up);
    lv_anim_start(&a);
    log_heap("sunrise");
}

/* ---- the schedule ---- */

static void start_night(void)
{
    if (app_manager_current()) {
        app_manager_home(); /* its close lands on the lock through bedtime_take_over(), without the mew */
        return;
    }
    if (lv_screen_active() != launcher_screen() || lv_display_get_screen_loading(NULL)) {
        return; /* the menu is still fading in: try again next second */
    }
    ui_cut_touch(launcher_screen()); /* a swipe or hold may be mid-touch on the menu (review M2) */
    show_lock(false); /* the menu screen stays for the morning */
}

static void tick(lv_timer_t *t)
{
    (void)t;
    const uint16_t now = clock_now_min();
    if (!app_manager_started() || app_manager_current() == &PARENT_APP) {
        return; /* the boot animation; parent changes take effect when the parent leaves (spec §3.12) */
    }
    if (schedule_sync_due(clock_valid(), now, settings_wake(), clock_secs_since_attempt())) {
        clock_sync_start(); /* the due sync: boot, daily before wake, or a retry while invalid (spec §3.2) */
    }
    const bool night = night_now();
    if (s_state == BT_DAY && night) {
        start_night();
    } else if (s_state == BT_LOCK && !night) {
        start_sunrise();
    } else if (s_state == BT_LOCK) {
        lock_tick(now);
    }
}

void bedtime_init(void)
{
    lv_timer_create(tick, BEDTIME_TICK_MS, NULL);
}

bool bedtime_take_over(void)
{
    if (s_state == BT_SUNRISE) {
        return false; /* not reached: nothing can open during the sunrise */
    }
    if (night_now()) {
        show_lock(true); /* the closing app's screen, or the boot screen, goes */
        return true;
    }
    if (s_state == BT_LOCK) {
        drop_lock(); /* the parent moved the times out of the night: the menu, without the sunrise */
    }
    return false;
}

bool bedtime_showing(void) { return s_state != BT_DAY && !app_manager_current(); }

void bedtime_power(void)
{
    if (s_state == BT_SUNRISE) {
#if CONFIG_CAT_HOME_ON_POWER
        bedtime_home(); /* PWR is this build's Home: the menu, lit (spec §3.12) */
#else
        finish_sunrise();
        idle_power_toggle(); /* the menu, then dark */
#endif
        return;
    }
    if (s_state != BT_LOCK) {
        return;
    }
    if (s_lit) {
        s_user_off = true;
        s_on_until = lv_tick_get();
        set_lit(false);
    } else {
        s_user_off = false;
        keep_lit(BEDTIME_ON_MS);
        update_panel();
    }
}

void bedtime_home(void)
{
    if (s_state == BT_SUNRISE) {
        finish_sunrise();
    }
}
