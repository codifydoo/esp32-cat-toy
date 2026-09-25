#include "parent.h"
#include <stdint.h>
#include <string.h>
#include "audio.h"
#include "cat.h"
#include "clock.h"
#include "first_run.h"
#include "fonts.h"
#include "power.h"
#include "power_rules.h"
#include "schedule.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

/* The parent area (spec §3.1, §3.12, §3.13): a home screen with the Volume, Sleep, Apps and Cat tiles and the
 * battery in the middle, then Max volume, Sleep times, Show apps and the cat picker. The pages share the one app
 * screen: a page switch cleans it and builds the next page. Home (BOOT) leaves the whole area. */

#define BACK_LABEL "\xE2\x80\xB9 Parents" /* "‹ Parents" */

typedef enum { PAGE_HOME, PAGE_VOLUME, PAGE_TIMES, PAGE_APPS, PAGE_CAT } page_t;
typedef enum { ROW_BED, ROW_WAKE, ROW_NOW, ROW_COUNT } row_t;

static const struct {
    const char *icon, *label;
} ROWS[ROW_COUNT] = {
    [ROW_BED] = {"ui/p_moon_row", "Bedtime"},
    [ROW_WAKE] = {"ui/p_sun_row", "Wake up"},
    [ROW_NOW] = {"ui/p_clock_row", "Now"},
};

/* Show apps' labels (the design's APPS), by app id; an app missing here shows its id. */
static const struct {
    const char *id, *label;
} APP_LABELS[] = {
    {"voices", "Voices"}, {"pet", "Pet"}, {"draw", "Draw"}, {"yarn", "Yarn"}, {"clock", "Clock"}, {"piano", "Piano"},
};

static const char *const CAT_NAMES[CAT_VARIANT_COUNT] = {"Orange", "Grey", "Tuxedo"}; /* the design's catNames */

static lv_obj_t *s_scr; /* NULL once destroyed: every callback checks it */
static page_t s_page;
static lv_obj_t *s_arc, *s_value;       /* Max volume */
static lv_obj_t *s_times[ROW_COUNT];    /* Sleep times: the three values; NULL off that page */
static lv_obj_t *s_batt, *s_batt_text;  /* the home's battery; NULL off the home */
static lv_timer_t *s_page_timer;        /* every second: Now on Sleep times, the battery on the home */
static uint16_t s_bed, s_wake;          /* edited on Sleep times, saved when a button is let go */
static uint32_t s_press_ms;             /* when the current ± press began */

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
    lv_obj_center(label_in(b, &font_fredoka_20, COLOR_P_TEXT, BACK_LABEL));
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)PAGE_HOME);
}

static uint8_t shown_apps(uint32_t hidden)
{
    uint8_t n = 0;
    for (size_t i = 0; i < APP_COUNT; i++) {
        n += (hidden & (1u << i)) ? 0 : 1;
    }
    return n;
}

/* ---- home ---- */

static void tile(int32_t x, int32_t y, const char *icon, const char *label, const char *value, page_t page)
{
    lv_obj_t *b = surface_button(x, y, PARENT_TILE_SIZE, PARENT_TILE_SIZE);
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)page);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, PARENT_TILE_PAD_ROW, 0);
    ui_image(b, icon, 0, 0); /* the flex layout places it */
    label_in(b, &font_fredoka_20, COLOR_P_TEXT, label);
    label_in(b, &font_fredoka_17, COLOR_P_VALUE, value);
}

/* The battery in the middle (spec §3.13): hidden without a gauge (§4.7), red at 10 % or less. */
static void show_battery(void)
{
    const power_state_t ps = power_state();
    ui_set_hidden(s_batt, ps.pct < 0);
    if (ps.pct < 0) {
        return;
    }
    const bool low = ps.pct <= POWER_LOW_PCT;
    lv_obj_set_style_border_color(s_batt, lv_color_hex(low ? COLOR_BATT_LOW : COLOR_CHARGE), 0);
    lv_obj_set_style_text_color(s_batt_text, lv_color_hex(low ? COLOR_BATT_LOW : COLOR_P_BATT_TEXT), 0);
    lv_label_set_text_fmt(s_batt_text, "%d%%", ps.pct);
}

static void build_home(void)
{
    text(s_scr, &font_fredoka_22, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_HOME_TITLE_Y, PARENT_TITLE_W, "Parents");
    char vol[8], bed[6], apps[12];
    lv_snprintf(vol, sizeof(vol), "%u%%", (unsigned)settings_volume_cap());
    schedule_format(settings_bedtime(), bed);
    lv_snprintf(apps, sizeof(apps), "%u of %u", (unsigned)shown_apps(settings_hidden_apps()), (unsigned)APP_COUNT);
    tile(PARENT_TILE_X1, PARENT_TILE_Y1, "ui/p_vol", "Volume", vol, PAGE_VOLUME);
    tile(PARENT_TILE_X2, PARENT_TILE_Y1, "ui/p_moon", "Sleep", bed, PAGE_TIMES);
    tile(PARENT_TILE_X1, PARENT_TILE_Y2, "ui/p_grid", "Apps", apps, PAGE_APPS);
    tile(PARENT_TILE_X2, PARENT_TILE_Y2, "ui/p_cat", "Cat", CAT_NAMES[settings_cat_variant() % CAT_VARIANT_COUNT],
         PAGE_CAT);
    s_batt = ui_plain(s_scr);
    lv_obj_set_pos(s_batt, PARENT_BATT_X, PARENT_BATT_Y);
    lv_obj_set_size(s_batt, PARENT_BATT_D, PARENT_BATT_D);
    lv_obj_set_style_radius(s_batt, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_batt, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_batt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_batt, PARENT_BATT_BORDER, 0);
    s_batt_text = label_in(s_batt, &font_fredoka_20, COLOR_P_BATT_TEXT, "");
    lv_obj_center(s_batt_text);
    show_battery();
    text(s_scr, &font_fredoka_17, COLOR_P_MUTED, PARENT_HINT_X, PARENT_HINT_Y, PARENT_HINT_W, "Home button exits");
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
    text(s_scr, &font_fredoka_20, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_VOL_TITLE_Y, PARENT_TITLE_W, "Max volume");
    s_arc = ui_arc(s_scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(s_scr, &font_fredoka_54, COLOR_P_TEXT, PARENT_VALUE_X, PARENT_VOL_VALUE_Y, PARENT_VALUE_W, "");
    lv_obj_t *m = surface_button(PARENT_MINUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(m, &font_fredoka_46, COLOR_P_TEXT, "-"));
    lv_obj_add_event_cb(m, on_minus, LV_EVENT_CLICKED, NULL);
    lv_obj_t *p = surface_button(PARENT_PLUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(p, &font_fredoka_46, COLOR_P_TEXT, "+"));
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

static void build_times(void)
{
    back_button();
    s_bed = settings_bedtime();
    s_wake = settings_wake();
    for (int r = 0; r < ROW_COUNT; r++) {
        const int32_t y = PARENT_ROW_Y0 + r * PARENT_ROW_STEP;
        lv_obj_t *head = ui_plain(s_scr); /* icon and label, centered as a pair */
        lv_obj_set_pos(head, PARENT_ROW_HEAD_X, y);
        lv_obj_set_size(head, PARENT_ROW_HEAD_W, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(head, PARENT_ROW_HEAD_PAD, 0);
        ui_image(head, ROWS[r].icon, 0, 0);
        label_in(head, &font_fredoka_20, COLOR_P_MUTED, ROWS[r].label);
        s_times[r] = text(s_scr, &font_fredoka_46, COLOR_P_TEXT, PARENT_VALUE_X, y + PARENT_ROW_VALUE_DY,
                          PARENT_VALUE_W, "");
        for (int d = 0; d < 2; d++) {
            lv_obj_t *b = surface_button(d ? PARENT_ROW_PLUS_X : PARENT_ROW_MINUS_X, y + PARENT_ROW_BTN_DY,
                                         PARENT_ROW_BTN, PARENT_ROW_BTN);
            lv_obj_center(label_in(b, &font_fredoka_46, COLOR_P_TEXT, d ? "+" : "-"));
            lv_obj_add_event_cb(b, time_button, LV_EVENT_ALL, (void *)(uintptr_t)(r * 2 + d));
        }
    }
    text(s_scr, &font_fredoka_17, COLOR_P_MUTED, PARENT_TIMES_NOTE_X, PARENT_TIMES_NOTE_Y, PARENT_TIMES_NOTE_W,
         "Outside these hours\nthe toy sleeps");
    show_times();
}

/* ---- Show apps ---- */

static const char *app_label(const app_t *a)
{
    for (size_t i = 0; i < sizeof(APP_LABELS) / sizeof(APP_LABELS[0]); i++) {
        if (strcmp(APP_LABELS[i].id, a->id) == 0) {
            return APP_LABELS[i].label;
        }
    }
    return a->id;
}

static void app_toggled(lv_event_t *e)
{
    if (!s_scr || s_page != PAGE_APPS) {
        return;
    }
    const uint32_t bit = 1u << (uintptr_t)lv_event_get_user_data(e);
    const uint32_t hidden = settings_hidden_apps();
    if (!(hidden & bit) && shown_apps(hidden) <= 1) {
        return; /* the last shown app stays (design: at most 5 of 6 hidden) */
    }
    settings_set_hidden_apps(hidden ^ bit); /* the menu shows it when the parent leaves (launcher_show) */
    lv_async_call(page_async, (void *)(uintptr_t)PAGE_APPS); /* rebuilt: the button this came from goes */
}

static void build_apps(void)
{
    back_button();
    text(s_scr, &font_fredoka_20, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_APPS_TITLE_Y, PARENT_TITLE_W, "Show apps");
    const uint32_t hidden = settings_hidden_apps();
    for (size_t i = 0; i < APP_COUNT && i < PARENT_APPS_MAX; i++) {
        const bool off = (hidden & (1u << i)) != 0;
        lv_obj_t *b = ui_plain(s_scr);
        lv_obj_set_pos(b, PARENT_APP_X0 + (int32_t)(i % 3) * PARENT_APP_DX,
                       PARENT_APP_Y0 + (int32_t)(i / 3) * PARENT_APP_DY);
        lv_obj_set_size(b, PARENT_APP_W, PARENT_APP_H);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_event_cb(b, app_toggled, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *icon = ui_image(b, APPS[i]->icon_small, (PARENT_APP_W - PARENT_APP_ICON) / 2, 0);
        lv_obj_set_style_image_opa(icon, off ? PARENT_APP_OFF_OPA : LV_OPA_COVER, 0);
        text(b, &font_fredoka_17, off ? COLOR_P_OFF : COLOR_P_TEXT, 0, PARENT_APP_LABEL_Y, PARENT_APP_W,
             app_label(APPS[i]));
        if (off) {
            ui_image(b, "ui/p_eye_off", PARENT_EYE_X, PARENT_EYE_Y);
        }
    }
}

/* ---- Cat ---- */

static void cat_chosen(void)
{
    lv_async_call(page_async, (void *)(uintptr_t)PAGE_HOME); /* the page switch deletes the picker */
}

/* ---- pages ---- */

static void page_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_scr) {
        return;
    }
    if (s_times[ROW_NOW]) {
        show_times();
    }
    if (s_batt) {
        show_battery();
    }
}

static void leave_page(void)
{
    if (s_times[ROW_BED]) {
        save_times();
    }
    s_arc = s_value = NULL;
    s_batt = s_batt_text = NULL;
    for (int r = 0; r < ROW_COUNT; r++) {
        s_times[r] = NULL;
    }
}

static void open_page(page_t page)
{
    leave_page();
    lv_obj_clean(s_scr);
    s_page = page;
    switch (page) {
    case PAGE_VOLUME: build_volume(); break;
    case PAGE_TIMES: build_times(); break;
    case PAGE_APPS: build_apps(); break;
    case PAGE_CAT: first_run_picker(s_scr, cat_chosen); break;
    default: build_home(); break;
    }
}

static void parent_create(lv_obj_t *scr)
{
    s_scr = scr;
    s_page_timer = lv_timer_create(page_tick, 1000, NULL);
    open_page(PAGE_HOME);
}

static void parent_destroy(void)
{
    leave_page(); /* saves the times: Home in the middle of a hold never delivers the release */
    if (s_page_timer) {
        lv_timer_delete(s_page_timer);
        s_page_timer = NULL;
    }
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
