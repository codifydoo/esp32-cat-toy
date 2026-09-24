#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "motion.h"
#include "tokens.h"
#include "ui_util.h"
#include "yarn_rules.h"

/* Yarn roll (design: Screen.dc.html yarn and yarnwin views, Main.dc.html tilt() and shake(); spec §3.10
 * tilt rules and §3.11). Tilt the toy to roll the yarn into the basket while the cat chases it. */

typedef enum { Y_PLAY, Y_HIT, Y_WIN } phase_t;

static const char *const FRAME[YARN_FRAMES] = {"ui/yarn_f0", "ui/yarn_f1", "ui/yarn_f2", "ui/yarn_f3"};
static const uint32_t CONFETTI_COLORS[] = RAINBOW_COLORS;
#define CONFETTI_COLOR_COUNT (sizeof(CONFETTI_COLORS) / sizeof(CONFETTI_COLORS[0]))

static lv_obj_t *s_yarn; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_screen, *s_basket, *s_cat, *s_hint, *s_win;
static lv_obj_t *s_confetti[YARN_CONFETTI];
static lv_timer_t *s_hint_timer, *s_win_timer, *s_round_timer, *s_rumble_timer;
static phase_t s_phase;
static uint8_t s_round;        /* which basket spot */
static int16_t s_bx, s_by;     /* the basket's center */
static float s_x, s_y;         /* the yarn's center */
static float s_cat_x, s_cat_y; /* the chasing cat's center */
static float s_look_x, s_look_y;
static uint8_t s_frame;
static float s_frame_px; /* rolled since the last frame change */
static bool s_rumbling;
static uint32_t s_last_tilt_ms;

/* ---- helpers ---- */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void cancel_timers(void)
{
    lv_timer_t **all[] = {&s_hint_timer, &s_win_timer, &s_round_timer, &s_rumble_timer};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (*all[i]) {
            lv_timer_delete(*all[i]);
            *all[i] = NULL;
        }
    }
}

static void rumble(bool on)
{
    if (on == s_rumbling) {
        return;
    }
    s_rumbling = on;
    if (on) {
        audio_loop_start(SND_RUMBLE);
    } else {
        audio_loop_stop();
    }
}

static void rumble_done(lv_timer_t *t)
{
    (void)t;
    s_rumble_timer = NULL; /* one-shot timers delete themselves */
    if (!s_yarn) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    rumble(false); /* also when on_tilt stopped coming (PWR, dimming) */
}

/* ---- tilt hint ---- */

static void wobble_step(void *obj, int32_t v) { lv_image_set_rotation(obj, v); }

static void hint_off(void)
{
    lv_anim_delete(s_hint, wobble_step);
    ui_set_hidden(s_hint, true);
}

static void hint_done(lv_timer_t *t)
{
    (void)t;
    s_hint_timer = NULL; /* one-shot timers delete themselves */
    if (!s_yarn) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    hint_off();
}

/* ---- the yarn and the chasing cat ---- */

static void place_yarn(void)
{
    lv_obj_set_pos(s_yarn, (int32_t)lroundf(s_x) - YARN_SIZE / 2, (int32_t)lroundf(s_y) - YARN_SIZE / 2);
}

/* The strands turn a quarter per YARN_FRAME_PX rolled, one way or the other. */
static void roll_frames(float moved, bool forward)
{
    s_frame_px += moved;
    if (s_frame_px < YARN_FRAME_PX) {
        return;
    }
    s_frame_px -= YARN_FRAME_PX;
    s_frame = (uint8_t)((s_frame + (forward ? 1 : YARN_FRAMES - 1)) % YARN_FRAMES);
    lv_image_set_src(s_yarn, assets_image(FRAME[s_frame]));
}

/* The cat eases toward its spot behind the yarn and looks at the yarn (design yarn view). snap: jump there. */
static void chase(uint32_t dt_ms, bool snap)
{
    float tx, ty;
    yarn_chase_target(s_x, s_y, SCREEN_CENTER, SCREEN_CENTER, &tx, &ty);
    s_cat_x = snap ? tx : yarn_follow(s_cat_x, tx, dt_ms);
    s_cat_y = snap ? ty : yarn_follow(s_cat_y, ty, dt_ms);
    cat_set_origin(s_cat, (int32_t)lroundf(s_cat_x) - YARN_CAT_DX, (int32_t)lroundf(s_cat_y) - YARN_CAT_DY);
    const float lx = (s_x - tx) / YARN_CHASE_PX, ly = (s_y - ty) / YARN_CHASE_PX; /* from its spot to the yarn */
    if (snap || fabsf(lx - s_look_x) + fabsf(ly - s_look_y) >= YARN_LOOK_STEP) {
        s_look_x = lx;
        s_look_y = ly;
        cat_set_look(s_cat, lx, ly);
    }
}

/* ---- rounds ---- */

static void start_round(void)
{
    s_phase = Y_PLAY;
    s_x = s_y = SCREEN_CENTER;
    yarn_basket(s_round, &s_bx, &s_by);
    lv_obj_set_pos(s_basket, s_bx - YARN_BASKET_W / 2, s_by - YARN_BASKET_H / 2);
    place_yarn();
    chase(0, true);
    ui_set_hidden(s_basket, false);
    ui_set_hidden(s_cat, false);
    ui_set_hidden(s_yarn, false);
}

static void confetti_step(void *obj, int32_t v) /* v: 0..1000 along the eased fall */
{
    lv_obj_set_style_translate_y(obj, YARN_CONFETTI_FALL * v / 1000, 0);
#if YARN_CONFETTI_SPIN
    lv_obj_set_style_transform_rotation(obj, YARN_CONFETTI_TURN * v / 1000, 0);
#endif
}

static void confetti_start(void)
{
    for (uint32_t i = 0; i < YARN_CONFETTI; i++) {
        lv_obj_t *k = ui_plain(s_win);
        lv_obj_set_size(k, YARN_CONFETTI_W, YARN_CONFETTI_H);
        lv_obj_set_pos(k, YARN_CONFETTI_X0 + (int32_t)(i * YARN_CONFETTI_X_STEP % YARN_CONFETTI_X_SPAN),
                       YARN_CONFETTI_TOP);
        lv_obj_set_style_radius(k, YARN_CONFETTI_R, 0);
        lv_obj_set_style_bg_color(k, lv_color_hex(CONFETTI_COLORS[i % CONFETTI_COLOR_COUNT]), 0);
        lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
        lv_obj_set_style_transform_pivot_x(k, YARN_CONFETTI_W / 2, 0);
        lv_obj_set_style_transform_pivot_y(k, YARN_CONFETTI_H / 2, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, k);
        lv_anim_set_exec_cb(&a, confetti_step);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, YARN_CONFETTI_MS);
        lv_anim_set_delay(&a, i * YARN_CONFETTI_DELAY % YARN_CONFETTI_DELAY_MAX);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
        s_confetti[i] = k;
    }
}

static void confetti_stop(void)
{
    for (size_t i = 0; i < YARN_CONFETTI; i++) {
        if (s_confetti[i]) {
            lv_anim_delete(s_confetti[i], confetti_step);
            s_confetti[i] = NULL;
        }
    }
}

static void next_round(lv_timer_t *t)
{
    (void)t;
    s_round_timer = NULL;
    if (!s_yarn) {
        return;
    }
    confetti_stop();
    lv_obj_delete(s_win); /* the confetti, the proud cat (and its timer) and the full basket */
    s_win = NULL;
    s_round++;
    start_round();
}

static void win_show(lv_timer_t *t)
{
    (void)t;
    s_win_timer = NULL;
    if (!s_yarn) {
        return;
    }
    s_phase = Y_WIN;
    hint_off();
    ui_set_hidden(s_yarn, true);
    ui_set_hidden(s_basket, true);
    ui_set_hidden(s_cat, true);
    s_win = ui_plain(s_screen);
    lv_obj_set_size(s_win, SCREEN_SIZE, SCREEN_SIZE);
    confetti_start(); /* first, so it falls behind the cat and the basket (design order) */
    lv_obj_t *cat = cat_create(s_win, CAT_SCALE_M);
    cat_set_origin(cat, YARN_WIN_CAT_X, YARN_WIN_CAT_Y);
    cat_set_state(cat, CAT_PROUD);
    ui_image(s_win, "ui/yarn_basket_full", YARN_FULL_BASKET_X, YARN_FULL_BASKET_Y);
    audio_play(SND_YARN_WIN);
    s_round_timer = one_shot(next_round, YARN_NEXT_ROUND_MS);
}

static void hit(void)
{
    s_phase = Y_HIT; /* the yarn stays put until the win scene (spec §3.11) */
    if (s_rumble_timer) {
        lv_timer_delete(s_rumble_timer);
        s_rumble_timer = NULL; /* it must not fire into the win */
    }
    rumble(false);
    s_win_timer = one_shot(win_show, YARN_WIN_DELAY_MS);
}

/* ---- sensors: app_manager calls these on the LVGL task while the screen is awake ---- */

static void yarn_tilt(float x_deg, float y_deg)
{
    if (!s_yarn) {
        return;
    }
    const uint32_t dt = lv_tick_elaps(s_last_tilt_ms);
    s_last_tilt_ms = lv_tick_get();
    if (s_phase != Y_PLAY) {
        return;
    }
    const float ox = s_x, oy = s_y;
    const float moved = roll_step(&s_x, &s_y, x_deg, y_deg, dt, SCREEN_CENTER, SCREEN_CENTER, YARN_RANGE);
    if (moved > 0.0f) {
        place_yarn();
    }
    if (moved >= YARN_ROLL_MIN_PX) {
        lv_display_trigger_activity(NULL); /* rolling is play; resting against the rim is not */
        roll_frames(moved, (s_x - ox) + (s_y - oy) >= 0.0f);
        rumble(true);
        if (!s_rumble_timer) {
            s_rumble_timer = one_shot(rumble_done, YARN_RUMBLE_STOP_MS);
        } else {
            lv_timer_reset(s_rumble_timer);
        }
    }
    chase(dt, false);
    if (yarn_in_basket(s_x, s_y, s_bx, s_by)) {
        hit();
    }
}

static void yarn_shake(void)
{
    if (!s_yarn || s_phase != Y_PLAY) {
        return; /* not during the win (spec §3.11) */
    }
    s_x = s_y = SCREEN_CENTER;
    place_yarn(); /* the cat eases after it on the next tilt sample */
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    /* design order: basket, cat, yarn, hint */
    s_basket = ui_image(scr, "ui/yarn_basket", 0, 0);
    s_cat = cat_create(scr, CAT_SCALE_S);
    cat_set_state(s_cat, CAT_SURPRISED);
    s_yarn = ui_image(scr, FRAME[0], 0, 0);
    s_hint = ui_image(scr, "ui/tilt_hint", YARN_HINT_X, YARN_HINT_Y);
    lv_image_set_pivot(s_hint, YARN_HINT_PIVOT_X, YARN_HINT_PIVOT_Y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_hint);
    lv_anim_set_exec_cb(&a, wobble_step);
    lv_anim_set_values(&a, -YARN_HINT_WOBBLE, YARN_HINT_WOBBLE);
    lv_anim_set_duration(&a, YARN_HINT_WOBBLE_MS / 2);
    lv_anim_set_reverse_duration(&a, YARN_HINT_WOBBLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void yarn_create(lv_obj_t *screen)
{
    s_screen = screen;
    s_round = 0;
    s_frame = 0;
    s_frame_px = 0.0f;
    s_rumbling = false;
    s_win = NULL;
    s_last_tilt_ms = lv_tick_get();
    build(screen);
    start_round();
    s_hint_timer = one_shot(hint_done, YARN_HINT_MS);
}

static void yarn_destroy(void)
{
    cancel_timers();
    lv_anim_delete(s_hint, wobble_step);
    confetti_stop();
    s_rumbling = false; /* the rumble loop: app_manager calls audio_stop_all() after this */
    s_yarn = NULL;
    s_screen = s_basket = s_cat = s_hint = s_win = NULL;
}

const app_t app_yarn = {
    .id = "yarn",
    .icon = "apps/yarn",
    .icon_small = "apps/yarn_96",
    .color = COLOR_YARN,
    .name_sound = SND_NAME_YARN,
    .swipe_sound = SND_SWIPE_YARN,
    .react = CAT_LISTENING,
    .create = yarn_create,
    .destroy = yarn_destroy,
    .on_shake = yarn_shake,
    .on_tilt = yarn_tilt,
};
