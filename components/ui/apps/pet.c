#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "gestures.h"
#include "motion.h"
#include "tokens.h"
#include "ui_util.h"

/* Pet the cat (design: Screen.dc.html pet view; spec §3.10). Tap = purr, rub = giggle,
 * shake = dizzy, tilt = a ball the cat's eyes follow, fish = eat. */

typedef enum { R_NONE, R_PURR, R_GIGGLE, R_DIZZY, R_EAT } react_t;

typedef struct {
    cat_state_t state;
    uint32_t ms; /* how long the cat keeps this face */
    bool hearts;
    sound_id_t sound; /* fx sound; the purr is a loop instead */
} reaction_t;

static const reaction_t REACTIONS[] = {
    [R_PURR] = {CAT_PURRING, PET_PURR_MS, true, SND_PURR},
    [R_GIGGLE] = {CAT_LAUGHING, PET_GIGGLE_MS, true, SND_PET_RUB},
    [R_DIZZY] = {CAT_DIZZY, PET_DIZZY_MS, false, SND_PET_DIZZY},
    [R_EAT] = {CAT_EATING, PET_EAT_MS, false, SND_PET_EAT},
};

/* Hearts around the cat (design: top-left corners and start delays). */
static const struct {
    int16_t x, y;
    uint16_t delay_ms;
} HEARTS[] = {{96, 70, 0}, {336, 60, 350}, {350, 190, 700}, {80, 200, 1000}};
#define HEART_COUNT (sizeof(HEARTS) / sizeof(HEARTS[0]))

static lv_obj_t *s_cat; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_ball, *s_fish_ring;
static lv_obj_t *s_hearts[HEART_COUNT];
static lv_timer_t *s_react_timer, *s_purr_timer;
static react_t s_react;
static rub_track_t s_rub;
static float s_ball_x, s_ball_y; /* ball center, screen px */
static bool s_ball_shown;
static uint32_t s_last_tilt_ms; /* last on_tilt outside the dead zone */
static uint32_t s_last_roll_ms; /* last on_tilt call */
static float s_tick_px;         /* rolled since the last bell tick */

/* ---- hearts ---- */

/* One float-up cycle, v = 0..1000 (design keyframes: from +10 px to -44 px, fading in over the
 * first 30 % and out over the rest). */
static void heart_step(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, 10 - (PET_HEART_RISE + 10) * v / 1000, 0);
    int32_t opa = v < 300 ? v * LV_OPA_COVER / 300 : (1000 - v) * LV_OPA_COVER / 700;
    lv_obj_set_style_opa(obj, (lv_opa_t)opa, 0);
}

static void hearts_show(bool show)
{
    for (size_t i = 0; i < HEART_COUNT; i++) {
        lv_obj_t *h = s_hearts[i];
        lv_anim_delete(h, heart_step);
        ui_set_hidden(h, !show);
        if (!show) {
            continue;
        }
        lv_obj_set_style_opa(h, LV_OPA_TRANSP, 0); /* invisible until its delay has passed */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, h);
        lv_anim_set_exec_cb(&a, heart_step);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, PET_HEART_MS);
        lv_anim_set_delay(&a, HEARTS[i].delay_ms);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

/* ---- the cat's face ---- */

/* Pupils toward the ball, only on the plain face (spec §3.10). */
static void look_at_ball(void)
{
    if (!s_ball_shown || s_react != R_NONE) {
        return;
    }
    const float dx = s_ball_x - PET_EYES_X, dy = s_ball_y - PET_EYES_Y;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) {
        return; /* right between the eyes: keep the last look */
    }
    cat_set_look(s_cat, dx / len, dy / len);
}

static void plain_face(void)
{
    s_react = R_NONE;
    hearts_show(false);
    lv_obj_set_style_opa(s_fish_ring, LV_OPA_COVER, 0);
    cat_set_state(s_cat, CAT_HAPPY); /* also drops any look */
    look_at_ball();
}

static void react_done(lv_timer_t *t)
{
    (void)t;
    s_react_timer = NULL; /* one-shot timers delete themselves */
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    plain_face();
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

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
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

/* A new reaction replaces whatever the cat was doing, sound included (spec §3.10). */
static void react(react_t r)
{
    const reaction_t *re = &REACTIONS[r];
    cancel_timers();
    audio_stop_all();
    s_react = r;
    cat_set_state(s_cat, re->state);
    hearts_show(re->hearts);
    lv_obj_set_style_opa(s_fish_ring, r == R_EAT ? PET_FISH_EAT_OPA : LV_OPA_COVER, 0);
    if (r == R_PURR) {
        audio_loop_start(re->sound);
        s_purr_timer = one_shot(purr_done, PET_PURR_SOUND_MS);
    } else {
        audio_play(re->sound);
    }
    s_react_timer = one_shot(react_done, re->ms);
}

/* ---- touch ---- */

static void cat_touch(lv_event_t *e)
{
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    const int16_t x = (int16_t)p.x, y = (int16_t)p.y;
    const uint32_t now = lv_tick_get();
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED: rub_begin(&s_rub, x, y, now); break;
    case LV_EVENT_PRESSING:
        if (rub_move(&s_rub, x, y)) {
            react(R_GIGGLE);
        }
        break;
    case LV_EVENT_RELEASED:
        if (rub_end(&s_rub, x, y, now)) {
            react(R_PURR);
        }
        break;
    default: break;
    }
}

static void fish_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return;
    }
    react(R_EAT);
}

/* ---- sensors: app_manager calls these on the LVGL task while the screen is awake ---- */

static void pet_shake(void)
{
    if (!s_cat) {
        return;
    }
    react(R_DIZZY);
}

static void pet_tilt(float x_deg, float y_deg)
{
    if (!s_cat) {
        return;
    }
    const uint32_t dt = lv_tick_elaps(s_last_roll_ms);
    s_last_roll_ms = lv_tick_get();
    if (tilt_is_level(x_deg, y_deg)) {
        if (s_ball_shown && lv_tick_elaps(s_last_tilt_ms) >= PET_BALL_HIDE_MS) {
            s_ball_shown = false;
            ui_set_hidden(s_ball, true);
            if (s_react == R_NONE) {
                cat_set_state(s_cat, CAT_HAPPY); /* eyes straight ahead again */
            }
        }
        return;
    }
    s_last_tilt_ms = s_last_roll_ms;
    if (!s_ball_shown) {
        s_ball_shown = true;
        ui_set_hidden(s_ball, false);
    }
    const float moved =
        roll_step(&s_ball_x, &s_ball_y, x_deg, y_deg, dt, SCREEN_CENTER, SCREEN_CENTER, PET_BALL_RANGE);
    lv_obj_set_pos(s_ball, (int32_t)lroundf(s_ball_x) - PET_BALL_SIZE / 2,
                   (int32_t)lroundf(s_ball_y) - PET_BALL_SIZE / 2);
    if (moved >= PET_ROLL_MIN_PX) {
        lv_display_trigger_activity(NULL); /* rolling is play; resting against the rim is not */
        s_tick_px += moved;
        if (s_tick_px >= PET_TICK_PX) {
            s_tick_px -= PET_TICK_PX;
            if (s_react == R_NONE) {
                audio_play(SND_BELL_TICK);
            }
        }
    }
    look_at_ball();
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    s_cat = cat_create(scr, CAT_SCALE_L);
    cat_set_origin(s_cat, PET_CAT_ORIGIN_X, PET_CAT_ORIGIN_Y);

    for (size_t i = 0; i < HEART_COUNT; i++) {
        s_hearts[i] = ui_image(scr, "ui/heart", HEARTS[i].x, HEARTS[i].y);
        ui_set_hidden(s_hearts[i], true);
    }

    /* Above the cat, below the fish (design order); not clickable, so taps reach the cat. */
    s_ball = ui_image(scr, "ui/ball", PET_BALL_START_X - PET_BALL_SIZE / 2, PET_BALL_START_Y - PET_BALL_SIZE / 2);
    ui_set_hidden(s_ball, true);

    lv_obj_t *hit = ui_circle(scr, PET_HIT_CX, PET_HIT_CY, PET_HIT_SIZE, 0);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_RELEASED, NULL);

    s_fish_ring = ui_circle(scr, PET_FISH_CX, PET_FISH_CY, PET_FISH_RING, COLOR_FISH);
    lv_obj_set_style_border_width(s_fish_ring, PET_FISH_BORDER, 0);
    lv_obj_set_style_border_color(s_fish_ring, lv_color_hex(COLOR_FISH_RING), 0);
    lv_obj_add_flag(s_fish_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_fish_ring, (PET_FISH_SIZE - PET_FISH_RING) / 2); /* the 150 px target */
    lv_obj_add_event_cb(s_fish_ring, fish_tapped, LV_EVENT_CLICKED, NULL);
    int16_t x, y;
    assets_ui_pos("ui/fish", &x, &y);
    /* not clickable: taps reach the ring */
    ui_image(scr, "ui/fish", PET_FISH_CX - PET_FISH_SIZE / 2 + x, PET_FISH_CY - PET_FISH_SIZE / 2 + y);
}

static void pet_create(lv_obj_t *screen)
{
    s_react = R_NONE;
    s_rub = (rub_track_t){0};
    s_ball_x = PET_BALL_START_X;
    s_ball_y = PET_BALL_START_Y;
    s_ball_shown = false;
    s_tick_px = 0.0f;
    s_last_roll_ms = s_last_tilt_ms = lv_tick_get();
    build(screen);
}

static void pet_destroy(void)
{
    cancel_timers();
    for (size_t i = 0; i < HEART_COUNT; i++) {
        lv_anim_delete(s_hearts[i], heart_step);
        s_hearts[i] = NULL;
    }
    s_cat = NULL;
    s_ball = NULL;
    s_fish_ring = NULL;
    /* the purr loop and any reaction sound: app_manager calls audio_stop_all() after this */
}

const app_t app_pet = {
    .id = "pet",
    .icon = "apps/pet",
    .icon_small = "apps/pet_96",
    .color = COLOR_PET,
    .name_sound = SND_NAME_PET,
    .swipe_sound = SND_SWIPE_PET,
    .react = CAT_HAPPY,
    .create = pet_create,
    .destroy = pet_destroy,
    .on_shake = pet_shake,
    .on_tilt = pet_tilt,
};
