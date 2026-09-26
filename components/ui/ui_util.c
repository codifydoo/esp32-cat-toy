#include "ui_util.h"
#include "assets.h"

lv_obj_t *ui_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_style_all(s);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
    return s;
}

lv_obj_t *ui_plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *ui_circle(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t d, uint32_t color)
{
    lv_obj_t *o = ui_plain(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, cx - d / 2, cy - d / 2);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

lv_obj_t *ui_image(lv_obj_t *parent, const char *asset, int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, assets_image(asset));
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(img, x, y);
    return img;
}

void ui_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *ui_arc(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, int32_t width)
{
    lv_obj_t *a = lv_arc_create(parent);
    int32_t d = 2 * r + width;
    lv_obj_set_size(a, d, d);
    lv_obj_set_pos(a, cx - d / 2, cy - d / 2);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}

void ui_wait_release(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) {
            lv_indev_wait_release(i);
        }
    }
}

/* A press already in flight on screen can otherwise still reach its event callbacks (e.g. a tap
 * handler) after the screen is replaced or destroyed. This is the same wait_release + reset pair
 * LVGL itself runs on every pointer indev before deleting an object it is currently pressing -- see
 * obj_indev_reset() in managed_components/lvgl__lvgl/src/core/lv_obj_tree.c. wait_release makes the
 * indev ignore the rest of the current touch; reset(indev, screen) drops this screen as the indev's
 * pressed/last/scrolled object so the next touch starts fresh once the finger lifts (see
 * indev_proc_press()/indev_proc_release() in lv_indev.c). */
void ui_cut_touch(lv_obj_t *screen)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            if (lv_indev_get_state(indev) != LV_INDEV_STATE_RELEASED) {
                lv_indev_wait_release(indev);
            }
            lv_indev_reset(indev, screen);
        }
        indev = lv_indev_get_next(indev);
    }
}
