#include "idle.h"
#include "app_manager.h"
#include "display.h"
#include "launcher.h"
#include "tokens.h"
#include "ui_util.h"

#define IDLE_TICK_MS 500

static uint8_t s_level;     /* 0 awake, 1 dim, 2 sleeping, 3 off */
static bool s_forced_off;   /* Power button: stays dark until a touch or a button */
static lv_obj_t *s_catcher; /* swallows the touch that wakes the screen */
static uint32_t s_last_shift;
static int s_anim_var; /* lv_anim needs a non-NULL var to identify the brightness fade */

static void set_brightness(void *var, int32_t v)
{
    (void)var;
    display_set_brightness((uint8_t)v);
}

static void fade_to(uint8_t pct, uint32_t ms)
{
    lv_anim_delete(&s_anim_var, set_brightness);
    if (ms == 0) {
        display_set_brightness(pct);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &s_anim_var);
    lv_anim_set_exec_cb(&a, set_brightness);
    lv_anim_set_values(&a, display_brightness(), pct);
    lv_anim_set_duration(&a, ms);
    lv_anim_start(&a);
}

static void catcher_event(lv_event_t *e)
{
    (void)e;
    idle_wake();
}

static void show_catcher(bool show)
{
    if (show && !s_catcher) {
        s_catcher = ui_plain(lv_layer_top());
        lv_obj_set_size(s_catcher, SCREEN_SIZE, SCREEN_SIZE);
        lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_catcher, catcher_event, LV_EVENT_RELEASED, NULL);
    } else if (!show && s_catcher) {
        lv_obj_delete_async(s_catcher);
        s_catcher = NULL;
    }
}

static void apply_level(uint8_t level)
{
    if (level == s_level) {
        return;
    }
    s_level = level;
    show_catcher(level > 0);
    if (level == 0) {
        fade_to(BRIGHTNESS_FULL, 0);
    } else if (level == 1) {
        fade_to(IDLE_DIM_PCT, ANIM_DIM_MS);
    } else if (level == 3) {
        fade_to(0, 0);
    }
    if (!app_manager_current()) {
        launcher_set_idle_level(level);
    }
}

void idle_wake(void)
{
    s_forced_off = false;
    lv_display_trigger_activity(NULL);
    apply_level(0);
}

bool idle_wake_if_asleep(void)
{
    if (s_level == 0) {
        return false;
    }
    idle_wake();
    return true;
}

void idle_power_toggle(void)
{
    if (s_level == 3) {
        idle_wake();
    } else {
        s_forced_off = true;
        apply_level(3);
    }
}

uint8_t idle_level(void) { return s_level; }

static void tick(lv_timer_t *t)
{
    (void)t;
    uint32_t inactive = lv_display_get_inactive_time(NULL);
    if (app_manager_current()) {
        if (inactive >= IDLE_APP_HOME_MS) {
            app_manager_home();
        }
        return;
    }
    if (!s_forced_off) {
        uint8_t level = inactive >= IDLE_OFF_MS     ? 3
                        : inactive >= IDLE_SLEEP_MS ? 2
                        : inactive >= IDLE_DIM_MS   ? 1
                                                    : 0;
        if (level > s_level) {
            apply_level(level); /* only escalate here; waking happens on touch or a button */
        }
    }
    if (lv_tick_elaps(s_last_shift) >= BURN_SHIFT_MS) {
        s_last_shift = lv_tick_get();
        launcher_burn_in_shift();
    }
}

void idle_init(void)
{
    s_last_shift = lv_tick_get();
    lv_timer_create(tick, IDLE_TICK_MS, NULL);
}
