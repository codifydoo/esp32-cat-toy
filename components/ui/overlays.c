#include "overlays.h"
#include <stddef.h>
#include "audio.h"
#include "cat.h"
#include "esp_log.h"
#include "tokens.h"
#include "ui_util.h"

static const char *TAG = "overlay";
static lv_obj_t *s_root; /* NULL when none shows */
static lv_timer_t *s_timer;
static void (*s_done)(void);

static void set_image_opa(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }
static void set_width(void *obj, int32_t v) { lv_obj_set_width(obj, v); }

/* A looping animation; `back` plays it back as well (the design's alternating keyframes). Deleting the object
 * deletes it. */
static void loop_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t ms, bool back)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    if (back) {
        lv_anim_set_reverse_duration(&a, ms);
    }
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void overlay_close(void)
{
    if (!s_root) {
        return;
    }
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    lv_obj_delete(s_root);
    s_root = NULL;
    void (*done)(void) = s_done;
    s_done = NULL;
    if (done) {
        done();
    }
}

static void timed_out(lv_timer_t *t)
{
    (void)t;
    s_timer = NULL; /* one-shot timers delete themselves */
    overlay_close();
}

void overlay_show(overlay_kind_t kind, void (*done)(void))
{
    if (s_root) {
        return;
    }
    const bool hungry = kind == OVERLAY_HUNGRY;
    ESP_LOGI(TAG, "%s", hungry ? "low battery" : "charging");
    s_done = done;
    s_root = ui_plain(lv_layer_top());
    lv_obj_set_size(s_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE); /* takes every touch: nothing reaches the app underneath */
    ui_wait_release(); /* nor does a touch already down: what it pressed gets PRESS_LOST when it lifts */
    lv_obj_t *cat = cat_create(s_root, CAT_SCALE_M);
    cat_set_origin(cat, OVERLAY_CAT_X, OVERLAY_CAT_Y);
    cat_set_state(cat, hungry ? CAT_SLEEPY : CAT_EATING);
    ui_image(s_root, hungry ? "ui/bowl_empty" : "ui/bowl_full", OVERLAY_BOWL_X,
             hungry ? OVERLAY_BOWL_EMPTY_Y : OVERLAY_BOWL_FULL_Y);
    if (hungry) {
        lv_obj_t *batt = ui_image(s_root, "ui/batt_low", OVERLAY_BATT_X, OVERLAY_BATT_Y);
        loop_anim(batt, set_image_opa, OVERLAY_BLINK_LOW_OPA, LV_OPA_COVER, OVERLAY_BLINK_MS / 2, true);
    } else {
        ui_image(s_root, "ui/batt_charge", OVERLAY_BATT_X, OVERLAY_BATT_Y);
        lv_obj_t *fill = ui_plain(s_root);
        lv_obj_set_pos(fill, OVERLAY_BATT_X + CHARGE_FILL_X, OVERLAY_BATT_Y + CHARGE_FILL_Y);
        lv_obj_set_size(fill, CHARGE_FILL_MIN_W, CHARGE_FILL_H);
        lv_obj_set_style_radius(fill, CHARGE_FILL_R, 0);
        lv_obj_set_style_bg_color(fill, lv_color_hex(COLOR_CHARGE), 0);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
        loop_anim(fill, set_width, CHARGE_FILL_MIN_W, CHARGE_FILL_W, CHARGE_FILL_MS, false);
        ui_image(s_root, "ui/batt_bolt", OVERLAY_BATT_X, OVERLAY_BATT_Y); /* over the fill */
    }
    audio_loop_stop(); /* a purr or rumble stops too: the app gets no touch or tilt meanwhile */
    audio_play(hungry ? SND_HUNGRY : SND_CHARGING);
    lv_display_trigger_activity(NULL);
    s_timer = lv_timer_create(timed_out, OVERLAY_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}

bool overlay_showing(void) { return s_root != NULL; }
