#include "parent.h"
#include <stdint.h>
#include "audio.h"
#include "clock.h"
#include "schedule.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

/* The parent area (spec §3.1, §3.12): a home screen with the Volume and Sleep tiles, Max volume and Sleep
 * times. The pages share the one app screen: a page switch cleans it and builds the next page. Home
 * (BOOT) leaves the whole area. */

typedef enum { PAGE_HOME, PAGE_VOLUME, PAGE_TIMES } page_t;
typedef enum { ROW_BED, ROW_WAKE, ROW_NOW, ROW_COUNT } row_t;

static const struct {
    const char *icon, *label;
} ROWS[ROW_COUNT] = {
    [ROW_BED] = {"ui/p_moon_row", "Bedtime"},
    [ROW_WAKE] = {"ui/p_sun_row", "Wake up"},
    [ROW_NOW] = {"ui/p_clock_row", "Now"},
};

static lv_obj_t *s_scr;              /* NULL once destroyed: every callback checks it */
static lv_obj_t *s_arc, *s_value;    /* Max volume */
static lv_obj_t *s_times[ROW_COUNT]; /* Sleep times: the three values; NULL off that page */
static lv_timer_t *s_now_timer;      /* keeps Now current while Sleep times shows */
static uint16_t s_bed, s_wake;       /* edited on Sleep times, saved when a button is let go */
static uint32_t s_press_ms;          /* when the current ± press began */

static void open_page(page_t page);

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

static lv_obj_t *surface_button(int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *b = lv_button_create(s_scr);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_SURFACE), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(COLOR_P_BORDER), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    return b;
}

static lv_obj_t *label_in(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

static void page_async(void *page)
{
    if (s_scr) {
        open_page((page_t)(uintptr_t)page);
    }
}

static void go_page(lv_event_t *e)
{
    /* Deferred: the page switch deletes the button this event came from. */
    lv_async_call(page_async, lv_event_get_user_data(e));
}

static void back_button(void)
{
    lv_obj_t *b = surface_button(PARENT_BACK_X, PARENT_BACK_Y, PARENT_BACK_W, PARENT_BACK_H);
    lv_obj_center(label_in(b, &lv_font_montserrat_20, COLOR_P_TEXT, LV_SYMBOL_LEFT " Parents"));
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)PAGE_HOME);
}

/* ---- home ---- */

static void tile(int32_t x, const char *icon, const char *label, const char *value, page_t page)
{
    lv_obj_t *b = surface_button(x, PARENT_TILE_Y, PARENT_TILE_SIZE, PARENT_TILE_SIZE);
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)page);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 2, 0);
    ui_image(b, icon, 0, 0); /* the flex layout places it */
    label_in(b, &lv_font_montserrat_20, COLOR_P_TEXT, label);
    label_in(b, &lv_font_montserrat_16, COLOR_P_VALUE, value);
}

static void build_home(void)
{
    text(s_scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 46, 200, "Parents");
    char vol[8], bed[6];
    lv_snprintf(vol, sizeof(vol), "%u%%", (unsigned)settings_volume_cap());
    schedule_format(settings_bedtime(), bed);
    tile(PARENT_TILE_VOL_X, "ui/p_vol", "Volume", vol, PAGE_VOLUME);
    tile(PARENT_TILE_SLEEP_X, "ui/p_moon", "Sleep", bed, PAGE_TIMES);
    text(s_scr, &lv_font_montserrat_16, COLOR_P_MUTED, 113, 398, 240, "Home button exits");
}

/* ---- Max volume ---- */

static void show_volume(void)
{
    const uint8_t v = settings_volume_cap();
    lv_arc_set_value(s_arc, v);
    lv_label_set_text_fmt(s_value, "%u%%", (unsigned)v);
}

static void step_volume(int dir)
{
    if (!s_scr || !s_arc) {
        return; /* destroyed, or the page is gone */
    }
    const uint8_t v = volume_step(settings_volume_cap(), dir);
    settings_set_volume_cap(v);
    audio_set_volume(v);
    audio_play(dir > 0 ? SND_BLIP_UP : SND_BLIP_DOWN); /* a click at the new level */
    show_volume();
}

static void on_minus(lv_event_t *e) { (void)e; step_volume(-1); }
static void on_plus(lv_event_t *e) { (void)e; step_volume(+1); }

static void build_volume(void)
{
    back_button();
    text(s_scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 98, 200, "Max volume");
    s_arc = ui_arc(s_scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(s_scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, 226, 160, "");
    lv_obj_t *m = surface_button(PARENT_MINUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(m, &lv_font_montserrat_40, COLOR_P_TEXT, "-"));
    lv_obj_add_event_cb(m, on_minus, LV_EVENT_CLICKED, NULL);
    lv_obj_t *p = surface_button(PARENT_PLUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(p, &lv_font_montserrat_40, COLOR_P_TEXT, "+"));
    lv_obj_add_event_cb(p, on_plus, LV_EVENT_CLICKED, NULL);
    show_volume();
}

/* ---- Sleep times ---- */

static void show_times(void)
{
    char t[6];
    schedule_format(s_bed, t);
    lv_label_set_text(s_times[ROW_BED], t);
    schedule_format(s_wake, t);
    lv_label_set_text(s_times[ROW_WAKE], t);
    if (clock_valid()) {
        schedule_format(clock_now_min(), t);
        lv_label_set_text(s_times[ROW_NOW], t);
    } else {
        lv_label_set_text(s_times[ROW_NOW], "--:--"); /* not set yet (spec §3.12) */
    }
}

static void save_times(void)
{
    if (s_bed != settings_bedtime()) {
        settings_set_bedtime(s_bed);
    }
    if (s_wake != settings_wake()) {
        settings_set_wake(s_wake);
    }
}

static void step_row(row_t row, int dir, uint32_t held_ms)
{
    if (row == ROW_BED) {
        s_bed = schedule_step(s_bed, dir * SCHEDULE_SLEEP_STEP);
    } else if (row == ROW_WAKE) {
        s_wake = schedule_step(s_wake, dir * SCHEDULE_SLEEP_STEP);
    } else if (!clock_valid()) {
        clock_set_now_min(SCHEDULE_NOW_START); /* the first press starts from 12:00 */
    } else {
        clock_set_now_min(schedule_step(clock_now_min(), dir * (int32_t)schedule_now_step(held_ms)));
    }
    show_times();
}

/* A ± button: one step on the press, then one per LVGL long-press repeat (every 100 ms after 400 ms).
 * User data: row * 2, plus 1 for +. */
static void time_button(lv_event_t *e)
{
    if (!s_scr || !s_times[ROW_BED]) {
        return; /* destroyed, or the page is gone */
    }
    const uintptr_t k = (uintptr_t)lv_event_get_user_data(e);
    const row_t row = (row_t)(k / 2);
    const int dir = (k & 1) ? 1 : -1;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        s_press_ms = lv_tick_get();
        step_row(row, dir, 0);
        break;
    case LV_EVENT_LONG_PRESSED_REPEAT: step_row(row, dir, lv_tick_elaps(s_press_ms)); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: save_times(); break;
    default: break;
    }
}

static void now_tick(lv_timer_t *t)
{
    (void)t;
    if (s_scr && s_times[ROW_NOW]) {
        show_times();
    }
}

static void build_times(void)
{
    back_button();
    s_bed = settings_bedtime();
    s_wake = settings_wake();
    for (int r = 0; r < ROW_COUNT; r++) {
        const int32_t y = PARENT_ROW_Y0 + r * PARENT_ROW_STEP;
        lv_obj_t *head = ui_plain(s_scr); /* icon and label, centered as a pair */
        lv_obj_set_pos(head, 113, y);
        lv_obj_set_size(head, 240, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(head, 8, 0);
        ui_image(head, ROWS[r].icon, 0, 0);
        label_in(head, &lv_font_montserrat_20, COLOR_P_MUTED, ROWS[r].label);
        s_times[r] = text(s_scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, y + PARENT_ROW_VALUE_DY, 160, "");
        for (int d = 0; d < 2; d++) {
            lv_obj_t *b = surface_button(d ? PARENT_ROW_PLUS_X : PARENT_ROW_MINUS_X, y + PARENT_ROW_BTN_DY,
                                         PARENT_ROW_BTN, PARENT_ROW_BTN);
            lv_obj_center(label_in(b, &lv_font_montserrat_40, COLOR_P_TEXT, d ? "+" : "-"));
            lv_obj_add_event_cb(b, time_button, LV_EVENT_ALL, (void *)(uintptr_t)(r * 2 + d));
        }
    }
    text(s_scr, &lv_font_montserrat_14, COLOR_P_MUTED, 118, PARENT_TIMES_NOTE_Y, 230,
         "Outside these hours\nthe toy sleeps");
    s_now_timer = lv_timer_create(now_tick, 1000, NULL);
    show_times();
}

/* ---- pages ---- */

static void leave_page(void)
{
    if (s_times[ROW_BED]) {
        save_times();
    }
    if (s_now_timer) {
        lv_timer_delete(s_now_timer);
        s_now_timer = NULL;
    }
    s_arc = s_value = NULL;
    for (int r = 0; r < ROW_COUNT; r++) {
        s_times[r] = NULL;
    }
}

static void open_page(page_t page)
{
    leave_page();
    lv_obj_clean(s_scr);
    if (page == PAGE_VOLUME) {
        build_volume();
    } else if (page == PAGE_TIMES) {
        build_times();
    } else {
        build_home();
    }
}

static void parent_create(lv_obj_t *scr)
{
    s_scr = scr;
    open_page(PAGE_HOME);
}

static void parent_destroy(void)
{
    leave_page(); /* saves the times: Home in the middle of a hold never delivers the release */
    s_scr = NULL;
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
