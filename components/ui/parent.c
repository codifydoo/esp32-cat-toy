#include "parent.h"
#include "audio.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

static lv_obj_t *s_arc, *s_value; /* NULL once destroyed: callbacks check s_arc */

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int32_t x, int32_t y,
                      int32_t w, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, txt);
    return l;
}

static void show_volume(void)
{
    uint8_t v = settings_volume_cap();
    lv_arc_set_value(s_arc, v);
    lv_label_set_text_fmt(s_value, "%u%%", (unsigned)v);
}

static void step(int dir)
{
    if (!s_arc) {
        return; /* destroy() already ran */
    }
    uint8_t v = volume_step(settings_volume_cap(), dir);
    settings_set_volume_cap(v);
    audio_set_volume(v);
    audio_play(dir > 0 ? SND_BLIP_UP : SND_BLIP_DOWN); /* a click at the new level */
    show_volume();
}

static void on_minus(lv_event_t *e) { (void)e; step(-1); }
static void on_plus(lv_event_t *e) { (void)e; step(+1); }

static void round_button(lv_obj_t *parent, int32_t x, const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, PARENT_BTN_Y);
    lv_obj_set_size(b, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_SURFACE), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(COLOR_P_BORDER), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COLOR_P_TEXT), 0);
    lv_label_set_text(l, sym);
    lv_obj_center(l);
}

static void parent_create(lv_obj_t *scr)
{
    /* The design's "< Parents" button leads to the tile grid, which this slice doesn't have yet. */
    text(scr, &lv_font_montserrat_16, COLOR_P_MUTED, 113, 52, 240, "Home button exits");
    text(scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 98, 200, "Max volume");
    s_arc = ui_arc(scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, 226, 160, "");
    round_button(scr, PARENT_MINUS_X, "-", on_minus);
    round_button(scr, PARENT_PLUS_X, "+", on_plus);
    show_volume();
}

static void parent_destroy(void)
{
    s_arc = NULL;
    s_value = NULL;
}

const app_t PARENT_APP = {
    .id = "parent",
    .icon = NULL,
    .icon_small = NULL,
    .color = COLOR_P_ARC,
    .name_sound = SND_BLIP_UP, /* unused: the parent area is never in the carousel */
    .swipe_sound = SND_BLIP_UP,
    .react = CAT_HAPPY,
    .create = parent_create,
    .destroy = parent_destroy,
    .on_shake = NULL,
    .on_tilt = NULL,
};
