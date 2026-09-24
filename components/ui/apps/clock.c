#include "app.h"
#include "audio.h"
#include "scenes.h"
#include "tokens.h"

/* Sleepy clock (spec §3.12): the day scene, and a tap on the cat purrs as in Pet the cat. The app is
 * never open at night: the bedtime lock closes it. */

static day_scene_t s_day; /* the rays read their points from here, so it outlives the screen */
static lv_obj_t *s_cat;   /* NULL once destroyed */
static lv_timer_t *s_react_timer, *s_purr_timer;

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void react_done(lv_timer_t *t)
{
    (void)t;
    s_react_timer = NULL; /* one-shot timers delete themselves */
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    cat_set_state(s_cat, CAT_PROUD);
}

static void purr_done(lv_timer_t *t)
{
    (void)t;
    s_purr_timer = NULL;
    if (!s_cat) {
        return;
    }
    audio_loop_stop();
}

static void cancel_timers(void)
{
    if (s_react_timer) {
        lv_timer_delete(s_react_timer);
        s_react_timer = NULL;
    }
    if (s_purr_timer) {
        lv_timer_delete(s_purr_timer);
        s_purr_timer = NULL;
    }
}

static void tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (p.x < DAY_CAT_X || p.x >= DAY_CAT_X + DAY_CAT_W || p.y < DAY_CAT_Y || p.y >= DAY_CAT_Y + DAY_CAT_H) {
        return; /* only the cat purrs */
    }
    cancel_timers();
    cat_set_state(s_cat, CAT_PURRING);
    audio_loop_start(SND_PURR);
    s_purr_timer = one_shot(purr_done, PET_PURR_SOUND_MS);
    s_react_timer = one_shot(react_done, PET_PURR_MS);
}

static void clock_create(lv_obj_t *screen)
{
    scene_day_create(&s_day, screen);
    s_cat = s_day.cat;
    lv_obj_add_event_cb(screen, tapped, LV_EVENT_CLICKED, NULL);
}

static void clock_destroy(void)
{
    cancel_timers();
    s_cat = NULL;
    /* the purr loop: app_manager calls audio_stop_all() after this */
}

const app_t app_clock = {
    .id = "clock",
    .icon = "apps/clock",
    .icon_small = "apps/clock_96",
    .color = COLOR_CLOCK,
    .name_sound = SND_NAME_CLOCK,
    .swipe_sound = SND_SWIPE_CLOCK,
    .react = CAT_SLEEPY,
    .create = clock_create,
    .destroy = clock_destroy,
    .on_shake = NULL,
    .on_tilt = NULL,
};
