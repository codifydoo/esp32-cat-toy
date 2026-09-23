#include "boot.h"
#include "audio.h"
#include "cat.h"
#include "tokens.h"
#include "ui_util.h"

#define RING_FROM 120
#define RING_TO   460

static void (*s_done)(void);

static void ring_size(void *obj, int32_t v)
{
    lv_obj_set_size(obj, v, v);
    lv_obj_center(obj);
    lv_obj_set_style_border_opa(obj, (lv_opa_t)(255 - v * 255 / (RING_TO + 20)), 0);
}

static void ring(lv_obj_t *scr, uint32_t delay_ms)
{
    lv_obj_t *r = ui_plain(scr);
    lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(r, 6, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(BOOT_RING_COLOR), 0);
    ring_size(r, RING_FROM);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, r);
    lv_anim_set_exec_cb(&a, ring_size);
    lv_anim_set_values(&a, RING_FROM, RING_TO);
    lv_anim_set_duration(&a, 1600);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void finish(lv_timer_t *t)
{
    (void)t;
    s_done();
}

void boot_run(void (*done)(void))
{
    s_done = done;
    lv_obj_t *scr = ui_screen();
    ring(scr, 0);
    ring(scr, 500);
    lv_obj_t *cat = cat_create(scr, CAT_SCALE_L);
    cat_set_origin(cat, PLACEHOLDER_CAT_X, PLACEHOLDER_CAT_Y);
    cat_set_state(cat, CAT_YAWN);
    lv_screen_load(scr);
    audio_play(SND_BOOT);
    lv_timer_t *t = lv_timer_create(finish, BOOT_MS, NULL);
    lv_timer_set_repeat_count(t, 1);
}
