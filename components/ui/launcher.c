#include "launcher.h"
#include <math.h>
#include <stdbool.h>
#include "app.h"
#include "app_manager.h"
#include "assets.h"
#include "audio.h"
#include "carousel.h"
#include "cat.h"
#include "esp_random.h"
#include "gestures.h"
#include "hold_ring.h"
#include "idle.h"
#include "parent.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"

#define MAX_APPS   32
#define DEG_TO_RAD (3.14159265f / 180.0f)

static lv_obj_t *s_scr, *s_cat, *s_icon, *s_icon_next, *s_static, *s_paw_a, *s_paw_b;
static lv_obj_t *s_dots[CAROUSEL_MAX_DOTS];
static const app_t *s_apps[MAX_APPS];
static carousel_t s_car;
static gesture_track_t s_track;
static bool s_busy;
static uint8_t s_idle_level;
static lv_timer_t *s_react_timer;

static const app_t *current(void) { return s_apps[s_car.index]; }

static cat_state_t resting_state(void)
{
    return s_idle_level >= 2 ? CAT_SLEEPING : s_idle_level == 1 ? CAT_SLEEPY : current()->react;
}

static void load_visible_apps(void)
{
    uint32_t hidden = settings_hidden_apps();
    uint8_t n = 0;
    for (size_t i = 0; i < APP_COUNT && n < MAX_APPS; i++) {
        if (!(hidden & (1u << i))) {
            s_apps[n++] = APPS[i];
        }
    }
    if (n == 0) { /* never show an empty menu */
        for (size_t i = 0; i < APP_COUNT && n < MAX_APPS; i++) {
            s_apps[n++] = APPS[i];
        }
    }
    uint8_t keep = s_car.index;
    carousel_init(&s_car, n);
    s_car.index = keep < n ? keep : 0;
}

static void layout_dots(void)
{
    dot_window_t w = carousel_dot_window(&s_car);
    for (uint8_t k = 0; k < CAROUSEL_MAX_DOTS; k++) {
        lv_obj_t *d = s_dots[k];
        if (k >= w.n) {
            ui_set_hidden(d, true);
            continue;
        }
        uint8_t i = (uint8_t)(w.first + k);
        dot_kind_t kind = carousel_dot_kind(&s_car, i);
        int32_t size = kind == DOT_ACTIVE ? MENU_DOT_ACTIVE : kind == DOT_EDGE ? MENU_DOT_EDGE : MENU_DOT_IDLE;
        float kk = (float)k - (float)(w.n - 1) / 2.0f;
        float a = (90.0f - kk * MENU_DOT_STEP_DEG) * DEG_TO_RAD;
        int32_t cx = SCREEN_CENTER + (int32_t)lroundf(MENU_DOT_RADIUS * cosf(a));
        int32_t cy = SCREEN_CENTER + (int32_t)lroundf(MENU_DOT_RADIUS * sinf(a));
        lv_obj_set_size(d, size, size);
        lv_obj_set_pos(d, cx - size / 2, cy - size / 2);
        lv_obj_set_style_bg_color(d, kind == DOT_ACTIVE ? lv_color_hex(s_apps[i]->color) : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(d, kind == DOT_ACTIVE ? LV_OPA_COVER : MENU_DOT_IDLE_OPA, 0);
        ui_set_hidden(d, false);
    }
}

static void react_later(lv_timer_t *t)
{
    (void)t;
    s_react_timer = NULL;
    cat_set_state(s_cat, resting_state());
}

static void say_name(bool with_whoosh)
{
    audio_play(with_whoosh ? current()->swipe_sound : current()->name_sound);
    cat_set_state(s_cat, CAT_TALKING);
    if (s_react_timer) {
        lv_timer_delete(s_react_timer);
    }
    s_react_timer = lv_timer_create(react_later, TALK_AFTER_NAME_MS, NULL);
    lv_timer_set_repeat_count(s_react_timer, 1);
}

static void set_scale(void *img, int32_t v) { lv_image_set_scale(img, (uint32_t)v); }
static void set_opa(void *obj, int32_t v) { lv_obj_set_style_opa(obj, (lv_opa_t)v, 0); }

static void run_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t ms,
                     lv_anim_path_cb_t path, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, path);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
    }
    lv_anim_start(&a);
}

static void slide_done(lv_anim_t *a)
{
    (void)a;
    ui_set_hidden(s_icon_next, true);
    run_anim(s_icon, set_scale, ICON_SCALE_SETTLE_FROM, LV_SCALE_NONE, ANIM_SETTLE_MS, lv_anim_path_overshoot, NULL);
    s_busy = false;
}

static void go(int8_t dir)
{
    if (s_car.count < 2) {
        say_name(false);
        return;
    }
    s_busy = true;
    carousel_step(&s_car, dir);
    lv_obj_t *out = s_icon, *in = s_icon_next;
    lv_image_set_src(in, assets_image(current()->icon));
    lv_image_set_scale(in, LV_SCALE_NONE);
    lv_obj_set_style_opa(in, LV_OPA_COVER, 0);
    lv_obj_set_x(in, MENU_ICON_X + dir * SCREEN_SIZE);
    ui_set_hidden(in, false);
    run_anim(out, (lv_anim_exec_xcb_t)lv_obj_set_x, MENU_ICON_X, MENU_ICON_X - dir * SCREEN_SIZE, ANIM_SWIPE_MS,
             lv_anim_path_ease_out, NULL);
    run_anim(in, (lv_anim_exec_xcb_t)lv_obj_set_x, MENU_ICON_X + dir * SCREEN_SIZE, MENU_ICON_X, ANIM_SWIPE_MS,
             lv_anim_path_ease_out, slide_done);
    s_icon = in;
    s_icon_next = out;
    layout_dots();
    say_name(true);
}

static void open_done(lv_anim_t *a)
{
    (void)a;
    app_manager_open(current());
}

static void open_current(void)
{
    s_busy = true;
    audio_play(SND_OPEN);
    run_anim(s_icon, set_scale, LV_SCALE_NONE, ICON_SCALE_OPEN, ANIM_OPEN_ZOOM_MS, lv_anim_path_ease_in, open_done);
    run_anim(s_icon, set_opa, LV_OPA_COVER, LV_OPA_TRANSP, ANIM_OPEN_ZOOM_MS, lv_anim_path_ease_in, NULL);
}

static bool in_circle(lv_point_t p, int32_t cx, int32_t cy, int32_t r)
{
    int32_t dx = p.x - cx, dy = p.y - cy;
    return dx * dx + dy * dy <= r * r;
}

static bool in_rect(lv_point_t p, int32_t x, int32_t y, int32_t w, int32_t h)
{
    return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
}

static void on_release(gesture_kind_t g, lv_point_t p)
{
    if (s_busy) {
        return;
    }
    if (g == GESTURE_SWIPE_LEFT) {
        go(+1);
    } else if (g == GESTURE_SWIPE_RIGHT) {
        go(-1);
    } else if (g == GESTURE_TAP) {
        if (in_circle(p, MENU_ICON_CX, MENU_ICON_CY, MENU_ICON_R)) {
            open_current();
        } else if (in_circle(p, MENU_CAT_CX, MENU_CAT_CY, MENU_CAT_R)) {
            say_name(false);
        } else if (in_rect(p, MENU_ZONE_LEFT_X, MENU_ZONE_Y, MENU_ZONE_W, MENU_ZONE_H)) {
            go(-1);
        } else if (in_rect(p, MENU_ZONE_RIGHT_X, MENU_ZONE_Y, MENU_ZONE_W, MENU_ZONE_H)) {
            go(+1);
        }
    }
}

static void screen_event(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    uint32_t now = lv_tick_get();
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED: gesture_begin(&s_track, (int16_t)p.x, (int16_t)p.y, now); break;
    case LV_EVENT_PRESSING: gesture_move(&s_track, (int16_t)p.x, (int16_t)p.y); break;
    case LV_EVENT_RELEASED: on_release(gesture_end(&s_track, (int16_t)p.x, (int16_t)p.y, now), p); break;
    default: break;
    }
}

static void chevron(const lv_point_precise_t *points)
{
    lv_obj_t *l = lv_line_create(s_static);
    lv_line_set_points(l, points, 3);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_line_width(l, 7, 0);
    lv_obj_set_style_line_color(l, lv_color_white(), 0);
    lv_obj_set_style_line_opa(l, STATIC_OPA, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
}

static void hold_holding(void)
{
    idle_wake_if_asleep(); /* show the ring even if the hold started on a dimmed or dark menu */
}

static void hold_done(void)
{
    idle_wake(); /* the hold may have started on a dimmed or dark menu */
    s_busy = true; /* launcher_show() clears it when the menu comes back */
    app_manager_open(&PARENT_APP);
}

void launcher_create(void)
{
    s_scr = ui_screen();
    lv_obj_add_event_cb(s_scr, screen_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_scr, screen_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_scr, screen_event, LV_EVENT_RELEASED, NULL);
    load_visible_apps();

    s_cat = cat_create(s_scr, CAT_SCALE_M);
    cat_set_origin(s_cat, MENU_CAT_ORIGIN_X, MENU_CAT_ORIGIN_Y);

    s_icon_next = ui_image(s_scr, current()->icon, MENU_ICON_X, MENU_ICON_Y);
    ui_set_hidden(s_icon_next, true);
    s_icon = ui_image(s_scr, current()->icon, MENU_ICON_X, MENU_ICON_Y);
    lv_image_set_pivot(s_icon, MENU_ICON_SIZE / 2, MENU_ICON_SIZE / 2);
    lv_image_set_pivot(s_icon_next, MENU_ICON_SIZE / 2, MENU_ICON_SIZE / 2);

    char paw[48];
    cat_part_asset(paw, sizeof(paw), CAT_SCALE_M, "paw");
    const lv_image_dsc_t *pd = assets_image(paw);
    s_paw_a = ui_image(s_scr, paw, MENU_PAW_A_CX - pd->header.w / 2, MENU_PAW_CY - pd->header.h / 2);
    s_paw_b = ui_image(s_scr, paw, MENU_PAW_B_CX - pd->header.w / 2, MENU_PAW_CY - pd->header.h / 2);

    s_static = ui_plain(s_scr); /* chevrons, dots and hint shift together against burn-in */
    lv_obj_set_size(s_static, SCREEN_SIZE, SCREEN_SIZE);
    static const lv_point_precise_t CHEVRON_L[] = {{54, 238}, {36, 262}, {54, 286}};
    static const lv_point_precise_t CHEVRON_R[] = {{412, 238}, {430, 262}, {412, 286}};
    chevron(CHEVRON_L);
    chevron(CHEVRON_R);
    for (int i = 0; i < CAROUSEL_MAX_DOTS; i++) {
        s_dots[i] = ui_circle(s_static, 0, 0, MENU_DOT_IDLE, 0xFFFFFF);
    }
    float a = HOME_HINT_ANGLE_DEG * DEG_TO_RAD;
    lv_obj_t *hint = ui_image(s_static, "ui/paw_hint",
                              SCREEN_CENTER + (int32_t)lroundf(HOME_HINT_RADIUS * cosf(a)) - 12,
                              SCREEN_CENTER + (int32_t)lroundf(HOME_HINT_RADIUS * sinf(a)) - 12);
    lv_obj_set_style_image_opa(hint, STATIC_OPA, 0);
    layout_dots();
    cat_set_state(s_cat, current()->react);
    static const hold_ring_cb_t HOLD = {.holding = hold_holding, .done = hold_done};
    hold_ring_attach(s_scr, HOLD_RING_CX, HOLD_RING_CY, &HOLD);
}

void launcher_show(void)
{
    load_visible_apps();
    lv_anim_delete(s_icon, NULL);
    lv_image_set_src(s_icon, assets_image(current()->icon));
    lv_image_set_scale(s_icon, LV_SCALE_NONE);
    lv_obj_set_style_opa(s_icon, LV_OPA_COVER, 0);
    lv_obj_set_x(s_icon, MENU_ICON_X);
    layout_dots();
    cat_set_state(s_cat, resting_state());
    s_busy = false;
    if (lv_screen_active() != s_scr) {
        lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_HOME_MS, 0, true);
    }
}

void launcher_home_pressed(void) { say_name(false); }

void launcher_set_idle_level(uint8_t level)
{
    s_idle_level = level;
    if (!s_react_timer) {
        cat_set_state(s_cat, resting_state());
    }
}

void launcher_burn_in_shift(void)
{
    int32_t span = 2 * BURN_SHIFT_PX + 1;
    lv_obj_set_pos(s_static, (int32_t)(esp_random() % span) - BURN_SHIFT_PX,
                   (int32_t)(esp_random() % span) - BURN_SHIFT_PX);
}

lv_obj_t *launcher_screen(void) { return s_scr; }

void launcher_cat_changed(void)
{
    char paw[48];
    cat_part_asset(paw, sizeof(paw), CAT_SCALE_M, "paw");
    lv_image_set_src(s_paw_a, assets_image(paw));
    lv_image_set_src(s_paw_b, assets_image(paw));
}
