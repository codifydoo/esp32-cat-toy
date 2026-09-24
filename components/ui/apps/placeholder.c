#include "app.h"
#include "audio.h"
#include "tokens.h"

/* A big cat that laughs and meows when tapped: the stand-in for apps not built yet. */

static lv_obj_t *s_cat;
static lv_timer_t *s_back;

static void back_to_happy(lv_timer_t *t)
{
    (void)t;
    s_back = NULL; /* one-shot timers delete themselves */
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    cat_set_state(s_cat, CAT_HAPPY);
}

static void tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    cat_set_state(s_cat, CAT_LAUGHING);
    audio_play(SND_MEOW);
    if (s_back) {
        lv_timer_delete(s_back);
    }
    s_back = lv_timer_create(back_to_happy, 1000, NULL);
    lv_timer_set_repeat_count(s_back, 1);
}

static void placeholder_create(lv_obj_t *screen)
{
    s_cat = cat_create(screen, CAT_SCALE_L);
    cat_set_origin(s_cat, PLACEHOLDER_CAT_X, PLACEHOLDER_CAT_Y);
    lv_obj_add_event_cb(screen, tapped, LV_EVENT_CLICKED, NULL);
}

static void placeholder_destroy(void)
{
    if (s_back) {
        lv_timer_delete(s_back);
        s_back = NULL;
    }
    s_cat = NULL;
}

#define PLACEHOLDER(sym, key, color_, name_snd, swipe_snd, react_) \
    const app_t sym = {                                            \
        .id = key,                                                 \
        .icon = "apps/" key,                                       \
        .icon_small = "apps/" key "_96",                           \
        .color = color_,                                           \
        .name_sound = name_snd,                                    \
        .swipe_sound = swipe_snd,                                  \
        .react = react_,                                           \
        .create = placeholder_create,                              \
        .destroy = placeholder_destroy,                            \
    };

/* Remove a line when that app's real file lands (voices: Task 17, pet: M4, draw, yarn and piano: M5). */
PLACEHOLDER(app_clock, "clock", COLOR_CLOCK, SND_NAME_CLOCK, SND_SWIPE_CLOCK, CAT_SLEEPY)
