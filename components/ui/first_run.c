#include "first_run.h"
#include <stdint.h>
#include <stdio.h>
#include "assets.h"
#include "audio.h"
#include "bedtime.h"
#include "cat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "launcher.h"
#include "sdkconfig.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"

static const char *TAG = "first_run";
static const uint32_t FUR[CAT_VARIANT_COUNT] = {COLOR_FUR_ORANGE, COLOR_FUR_GREY, COLOR_FUR_TUX};
static const int16_t CENTER[CAT_VARIANT_COUNT][2] = PICK_CENTERS;

static lv_obj_t *s_root; /* the picker's container; NULL when none is built */
static lv_obj_t *s_ring[CAT_VARIANT_COUNT], *s_cat[CAT_VARIANT_COUNT];
static lv_timer_t *s_timer; /* the celebration, then done */
static void (*s_done)(void);
static bool s_chosen;
static lv_obj_t *s_screen; /* the first-run screen while it shows */

/* The design's pad circle: r 72 with its stroke centered on it (5 px, 10 px and the tinted fill when chosen). */
static void style_ring(int v, bool chosen)
{
    const int32_t sw = chosen ? PICK_RING_THICK : PICK_RING_THIN;
    const int32_t d = 2 * PICK_RING_R + sw;
    lv_obj_set_size(s_ring[v], d, d);
    lv_obj_set_pos(s_ring[v], (PICK_PAD - d) / 2, (PICK_PAD - d) / 2);
    lv_obj_set_style_border_width(s_ring[v], sw, 0);
    lv_obj_set_style_bg_opa(s_ring[v], chosen ? PICK_FILL_OPA : LV_OPA_TRANSP, 0);
}

static void finish(lv_timer_t *t)
{
    (void)t;
    s_timer = NULL; /* one-shot timers delete themselves */
    if (s_done) {
        s_done();
    }
}

static void pad_clicked(lv_event_t *e)
{
    if (!s_root || s_chosen) {
        return; /* gone, or already chosen: one choice per picker */
    }
    const int v = (int)(uintptr_t)lv_event_get_user_data(e);
    s_chosen = true;
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        style_ring(i, i == v);
    }
    cat_set_state(s_cat[v], CAT_PROUD);
    audio_play(SND_MEOW);
    settings_set_cat_variant((uint8_t)v);
    settings_set_first_run_done();
    cat_set_variant((cat_variant_t)v); /* the menu's cat, and the lock's if it is built, switch now */
    launcher_cat_changed();
    ESP_LOGI(TAG, "cat chosen: %s", cat_variant_name((cat_variant_t)v));
    s_timer = lv_timer_create(finish, PICK_CELEBRATE_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}

/* Runs after the picker's deletion has finished, so no object shows the other variants any more (spec §4.6:
 * one variant in memory). */
static void release_others(void *arg)
{
    (void)arg;
    const int keep = settings_cat_variant();
    char prefix[16];
    for (int v = 0; v < CAT_VARIANT_COUNT; v++) {
        if (v != keep) {
            snprintf(prefix, sizeof(prefix), "cat/%s/", cat_variant_name((cat_variant_t)v));
            assets_release_prefix(prefix);
        }
    }
    ESP_LOGI(TAG, "free psram after the cat choice: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void picker_deleted(lv_event_t *e)
{
    (void)e;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_root = NULL;
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        s_ring[i] = s_cat[i] = NULL;
    }
    lv_async_call(release_others, NULL);
}

void first_run_picker(lv_obj_t *parent, void (*done)(void))
{
    s_done = done;
    s_chosen = false;
    s_root = ui_plain(parent);
    lv_obj_set_size(s_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_add_event_cb(s_root, picker_deleted, LV_EVENT_DELETE, NULL);
    const int current = settings_cat_variant();
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        lv_obj_t *pad = ui_plain(s_root);
        lv_obj_set_size(pad, PICK_PAD, PICK_PAD);
        lv_obj_set_pos(pad, CENTER[i][0] - PICK_PAD / 2, CENTER[i][1] - PICK_PAD / 2);
        lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_event_cb(pad, pad_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_ring[i] = ui_plain(pad);
        lv_obj_set_style_radius(s_ring[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(s_ring[i], lv_color_hex(FUR[i]), 0);
        lv_obj_set_style_bg_color(s_ring[i], lv_color_hex(FUR[i]), 0);
        style_ring(i, i == current);
        s_cat[i] = cat_create_colored(pad, CAT_SCALE_S, cat_variant_name((cat_variant_t)i));
        cat_set_origin(s_cat[i], PICK_CAT_DX, PICK_CAT_DY);
    }
}

bool first_run_due(void)
{
#if CONFIG_CAT_DEV_SOAK
    return false;
#else
    return !settings_first_run_done();
#endif
}

static void end_first_run(void)
{
    s_screen = NULL;
    if (!bedtime_take_over()) { /* night: the lock, which deletes this screen */
        launcher_show();        /* deletes this screen once the menu has faded in */
    }
}

void first_run_show(void)
{
    ESP_LOGI(TAG, "first run: the cat picker");
    s_screen = ui_screen();
    first_run_picker(s_screen, end_first_run);
    lv_screen_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_HOME_MS, 0, true); /* the boot screen goes */
}

bool first_run_showing(void) { return s_screen != NULL; }

void first_run_skip(void)
{
    if (!s_screen) {
        return;
    }
    ui_cut_touch(s_screen);
    end_first_run();
}
