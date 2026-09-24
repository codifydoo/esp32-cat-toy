#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "brush.h"
#include "cat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "paint.h"
#include "tokens.h"
#include "ui_util.h"

/* Magic paws (design: Screen.dc.html draw and erase views, Main.dc.html act('draw' | 'tool') and
 * shake(); spec §3.11). Rainbow dots or paw prints under her finger; a shake sweeps them away. */

static const char *TAG = "draw";
static const uint32_t RAINBOW[BRUSH_COLORS] = RAINBOW_COLORS;

/* The picture and its brush outlive the app on purpose: she finds her drawing again until she
 * shakes it clean or the toy restarts (spec §3.11, the §4.4 exception). */
static uint16_t *s_pixels;
static brush_t s_brush;
static bool s_empty = true;

static lv_obj_t *s_canvas; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_screen, *s_button, *s_hint, *s_dust, *s_sweeper;
static lv_timer_t *s_cisto_timer, *s_clean_timer;
static uint16_t s_colors[BRUSH_COLORS];
static bool s_sweeping;
static uint32_t s_sound_ms; /* last sparkle or pat */

/* ---- helpers ---- */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void cancel_timers(void)
{
    if (s_cisto_timer) {
        lv_timer_delete(s_cisto_timer);
        s_cisto_timer = NULL;
    }
    if (s_clean_timer) {
        lv_timer_delete(s_clean_timer);
        s_clean_timer = NULL;
    }
}

static void opa_step(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }

/* Shows or hides an image; shown, it twinkles like the design's .twinkle (30 % -> 100 % -> 30 %, 3 s). */
static void twinkle(lv_obj_t *img, bool on)
{
    lv_anim_delete(img, opa_step);
    ui_set_hidden(img, !on);
    if (!on) {
        return;
    }
    lv_obj_set_style_image_opa(img, DRAW_TWINKLE_LOW_OPA, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, opa_step);
    lv_anim_set_values(&a, DRAW_TWINKLE_LOW_OPA, LV_OPA_COVER);
    lv_anim_set_duration(&a, DRAW_TWINKLE_MS / 2);
    lv_anim_set_reverse_duration(&a, DRAW_TWINKLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static const char *button_image(void)
{
    return s_brush.kind == BRUSH_RAINBOW ? "ui/brush_rainbow" : "ui/brush_paw"; /* shows the current brush */
}

/* ---- painting ---- */

static void clear_picture(void)
{
    memset(s_pixels, 0, SCREEN_SIZE * SCREEN_SIZE * sizeof(uint16_t));
    brush_clear(&s_brush);
    s_empty = true;
}

static void paint_stamps(const stamp_t *st, size_t n)
{
    paint_rect_t d;
    paint_rect_reset(&d);
    for (size_t i = 0; i < n; i++) {
        const uint16_t c = s_colors[st[i].color];
        const float x = st[i].x + 0.5f, y = st[i].y + 0.5f; /* the middle of the touched pixel */
        if (s_brush.kind == BRUSH_RAINBOW) {
            paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, x, y, DRAW_HALO_R, c, DRAW_HALO_OPA, &d);
            paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, x, y, DRAW_DOT_R, c, LV_OPA_COVER, &d);
        } else {
            blob_t b[5];
            paw_blobs(&st[i], b);
            for (int k = 0; k < 5; k++) {
                paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, b[k].x + 0.5f, b[k].y + 0.5f, b[k].r, c,
                             LV_OPA_COVER, &d);
            }
        }
    }
    if (d.x1 < d.x0) {
        return; /* nothing changed */
    }
    lv_area_t a;
    lv_obj_get_coords(s_canvas, &a);
    const lv_area_t changed = {a.x1 + d.x0, a.y1 + d.y0, a.x1 + d.x1, a.y1 + d.y1};
    lv_obj_invalidate_area(s_canvas, &changed);
    if (s_empty) {
        s_empty = false;
        twinkle(s_hint, false);
    }
}

/* A sparkle every DRAW_SPARKLE_EVERY dots, a pat per print; each at most 8 per second (spec §3.11). */
static void stroke_sound(uint32_t dots_before, uint32_t paws_before)
{
    const bool rainbow = s_brush.kind == BRUSH_RAINBOW;
    const bool due = rainbow ? s_brush.dots / DRAW_SPARKLE_EVERY != dots_before / DRAW_SPARKLE_EVERY
                             : s_brush.paws != paws_before;
    if (!due || lv_tick_elaps(s_sound_ms) < DRAW_SOUND_GAP_MS) {
        return;
    }
    audio_play(rainbow ? SND_SPARKLE : SND_PAT);
    s_sound_ms = lv_tick_get();
}

/* ---- touch ---- */

static void touch(lv_event_t *e)
{
    if (!s_canvas || !s_pixels || s_sweeping) {
        return; /* closed (app.h), no picture, or the cat is sweeping (spec §3.11) */
    }
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        brush_up(&s_brush);
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(s_canvas, &a);
    const int16_t x = (int16_t)(p.x - a.x1), y = (int16_t)(p.y - a.y1);
    const uint32_t dots = s_brush.dots, paws = s_brush.paws;
    stamp_t st[BRUSH_MAX_STAMPS];
    const size_t n = code == LV_EVENT_PRESSED ? brush_down(&s_brush, x, y, st) : brush_move(&s_brush, x, y, st);
    paint_stamps(st, n);
    stroke_sound(dots, paws);
}

static void tool_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_canvas || s_sweeping) {
        return;
    }
    s_brush.kind = s_brush.kind == BRUSH_RAINBOW ? BRUSH_PAWS : BRUSH_RAINBOW;
    lv_image_set_src(s_button, assets_image(button_image()));
    audio_play(SND_BLIP_UP);
}

/* ---- sweep (shake) ---- */

static void sweep_x(void *obj, int32_t v) { lv_obj_set_style_translate_x(obj, v, 0); }

static void say_cisto(lv_timer_t *t)
{
    (void)t;
    s_cisto_timer = NULL; /* one-shot timers delete themselves */
    if (!s_canvas) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    audio_play(SND_CISTO);
}

static void sweep_done(lv_timer_t *t)
{
    (void)t;
    s_clean_timer = NULL;
    if (!s_canvas) {
        return;
    }
    s_sweeping = false;
    if (s_pixels) {
        clear_picture();
    }
    lv_anim_delete(s_sweeper, sweep_x);
    lv_obj_delete(s_sweeper);
    s_sweeper = NULL;
    twinkle(s_dust, false);
    lv_obj_set_style_image_opa(s_canvas, LV_OPA_COVER, 0);
    lv_obj_invalidate(s_canvas);
    ui_set_hidden(s_button, false);
    twinkle(s_hint, true);
}

static void draw_shake(void)
{
    if (!s_canvas || s_sweeping) {
        return;
    }
    s_sweeping = true;
    brush_up(&s_brush);
    lv_obj_set_style_image_opa(s_canvas, DRAW_SWEEP_OPA, 0);
    ui_set_hidden(s_button, true);
    twinkle(s_hint, false);

    s_sweeper = cat_create(s_screen, CAT_SCALE_M);
    cat_set_origin(s_sweeper, DRAW_SWEEP_CAT_X, DRAW_SWEEP_CAT_Y);
    cat_set_state(s_sweeper, CAT_SWEEP);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_sweeper);
    lv_anim_set_exec_cb(&a, sweep_x);
    lv_anim_set_values(&a, -DRAW_SWEEP_PX, DRAW_SWEEP_PX);
    lv_anim_set_duration(&a, DRAW_SWING_MS / 2);
    lv_anim_set_reverse_duration(&a, DRAW_SWING_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    twinkle(s_dust, true);
    lv_obj_move_to_index(s_dust, -1); /* design order: the dust over the cat */
    audio_play(SND_SWISH);
    s_cisto_timer = one_shot(say_cisto, DRAW_CISTO_MS);
    s_clean_timer = one_shot(sweep_done, DRAW_CLEAN_MS);
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    s_canvas = lv_canvas_create(scr);
    lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE); /* touches go to the screen */
    lv_obj_set_pos(s_canvas, 0, 0);
    if (s_pixels) {
        lv_canvas_set_buffer(s_canvas, s_pixels, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    }
    s_hint = ui_image(scr, "ui/draw_hint", DRAW_HINT_X, DRAW_HINT_Y);
    twinkle(s_hint, s_empty);
    s_dust = ui_image(scr, "ui/dust", DRAW_DUST_X, DRAW_DUST_Y);
    ui_set_hidden(s_dust, true);
    s_button = ui_image(scr, button_image(), DRAW_BUTTON_X, DRAW_BUTTON_Y);
    lv_obj_add_flag(s_button, LV_OBJ_FLAG_CLICKABLE); /* presses on it never reach the screen */
    /* Unlocked, a press that starts on the button and slides off loses it: no brush switch on release,
     * and the screen only gets PRESSING (never PRESSED), so nothing is painted either (design: click =
     * press and release on the button). */
    lv_obj_remove_flag(s_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(s_button, tool_tapped, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESS_LOST, NULL);
    /* A stroke keeps its finger across the brush button (design: setPointerCapture). A screen has no
     * press lock by default (LVGL only locks objects with a parent). */
    lv_obj_add_flag(scr, LV_OBJ_FLAG_PRESS_LOCK);
}

static void draw_create(lv_obj_t *screen)
{
    s_screen = screen;
    s_sweeping = false;
    s_sound_ms = 0;
    for (size_t i = 0; i < BRUSH_COLORS; i++) {
        s_colors[i] = paint_rgb565(RAINBOW[i]);
    }
    if (!s_pixels) {
        /* The one PSRAM block an app keeps after destroy() (spec §3.11, §4.4). */
        s_pixels = heap_caps_calloc(SCREEN_SIZE * SCREEN_SIZE, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!s_pixels) {
            ESP_LOGE(TAG, "no PSRAM for the picture: drawing is off until the next open");
        }
        brush_init(&s_brush, BRUSH_RAINBOW, SCREEN_CENTER, SCREEN_CENTER);
        s_empty = true;
    }
    brush_up(&s_brush);
    build(screen);
}

static void draw_destroy(void)
{
    cancel_timers();
    if (s_sweeping) {
        s_sweeping = false;
        if (s_pixels) {
            clear_picture(); /* she already shook it clean (spec §3.11) */
        }
    }
    if (s_sweeper) {
        lv_anim_delete(s_sweeper, sweep_x);
    }
    lv_anim_delete(s_dust, opa_step);
    lv_anim_delete(s_hint, opa_step);
    brush_up(&s_brush);
    s_canvas = NULL;
    s_screen = s_button = s_hint = s_dust = s_sweeper = NULL;
    /* the swish or "Čisto!": app_manager calls audio_stop_all() after this */
}

const app_t app_draw = {
    .id = "draw",
    .icon = "apps/draw",
    .icon_small = "apps/draw_96",
    .color = COLOR_DRAW,
    .name_sound = SND_NAME_DRAW,
    .swipe_sound = SND_SWIPE_DRAW,
    .react = CAT_SURPRISED,
    .create = draw_create,
    .destroy = draw_destroy,
    .on_shake = draw_shake,
};
