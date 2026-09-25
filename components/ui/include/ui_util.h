#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

lv_obj_t *ui_screen(void);                  /* black, clickable, not scrollable */
lv_obj_t *ui_plain(lv_obj_t *parent);       /* no style, not clickable, not scrollable */
lv_obj_t *ui_circle(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t d, uint32_t color);
lv_obj_t *ui_image(lv_obj_t *parent, const char *asset, int32_t x, int32_t y); /* not clickable */
void ui_set_hidden(lv_obj_t *obj, bool hidden);
/* A non-clickable arc without a knob, centered on (cx, cy) with the given radius to the middle of
 * the stroke; main (track) and indicator both get `width` and round caps. */
lv_obj_t *ui_arc(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, int32_t width);
/* Cuts a touch already in flight on screen: call before deleting or replacing a screen that may own
 * a pressed pointer indev, so the rest of that touch cannot still reach it. */
void ui_cut_touch(lv_obj_t *screen);
