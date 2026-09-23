#include "sdkconfig.h"
#if CONFIG_CAT_DEV_GALLERY
#include "gallery.h"
#include "cat.h"
#include "display.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sensors.h"

static const char *TAG = "GALLERY";
static const char *const COSTUME_NAME[CAT_COSTUME_COUNT] = {"none", "mouse", "lion", "robot", "miner"};
static const char SCALE_NAME[CAT_SCALE_COUNT] = {'S', 'M', 'L'};
static const int16_t ORIGIN_X[CAT_SCALE_COUNT] = {168, 133, 103}; /* head centered at x = 233 */
static const int16_t ORIGIN_Y[CAT_SCALE_COUNT] = {140, 110, 80};
static lv_obj_t *s_scr, *s_cat, *s_label;
static cat_state_t s_state;
static cat_costume_t s_costume;
static uint8_t s_combo; /* variant = combo % 3, scale = combo / 3 */

static void refresh_label(void)
{
    lv_label_set_text_fmt(s_label, "%s / %s / %c%u", cat_state_name(s_state), COSTUME_NAME[s_costume],
                          SCALE_NAME[s_combo / 3], s_combo % 3);
    ESP_LOGI(TAG, "state=%s costume=%s scale=%c variant=%u", cat_state_name(s_state), COSTUME_NAME[s_costume],
             SCALE_NAME[s_combo / 3], s_combo % 3);
}

static void rebuild(void)
{
    if (s_cat) {
        lv_obj_delete(s_cat);
    }
    cat_set_variant((cat_variant_t)(s_combo % 3));
    cat_scale_t scale = (cat_scale_t)(s_combo / 3);
    s_cat = cat_create(s_scr, scale);
    cat_set_origin(s_cat, ORIGIN_X[scale], ORIGIN_Y[scale]);
    cat_set_state(s_cat, s_state);
    cat_set_costume(s_cat, s_costume);
    lv_obj_move_background(s_cat);
    refresh_label();
}

static void tapped(lv_event_t *e)
{
    (void)e;
    s_state = (cat_state_t)((s_state + 1) % CAT_STATE_COUNT);
    cat_set_state(s_cat, s_state);
    refresh_label();
}

static void on_button(button_id_t id)
{
    if (!display_lock(-1)) {
        return;
    }
    if (id == BUTTON_HOME) {
        s_costume = (cat_costume_t)((s_costume + 1) % CAT_COSTUME_COUNT);
        cat_set_costume(s_cat, s_costume);
        refresh_label();
    } else {
        s_combo = (uint8_t)((s_combo + 1) % 9);
        rebuild();
    }
    display_unlock();
}

void gallery_start(void)
{
    display_lock(-1);
    s_scr = lv_screen_active();
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_scr, tapped, LV_EVENT_CLICKED, NULL);
    s_label = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    s_combo = 3; /* orange, M */
    rebuild();
    display_unlock();
    const sensors_listener_t listener = {.on_button = on_button};
    sensors_start(&listener);
}
#endif
