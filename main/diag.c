#include "sdkconfig.h"
#if CONFIG_CAT_DEV_DIAG
#include "diag.h"
#include "assets.h"
#include "audio.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "imu.h"
#include "lvgl.h"
#include "sensors.h"

#define DIAG_RECORD_S 3

static const char *TAG = "DIAG";
static lv_obj_t *s_touch, *s_accel, *s_buttons, *s_rec_label;
static uint32_t s_home, s_power, s_ticks, s_session;
static volatile bool s_busy;

static void set_rec_label(const char *text)
{
    if (display_lock(-1)) {
        lv_label_set_text(s_rec_label, text);
        display_unlock();
    }
}

static void on_recorded(uint32_t session, int16_t *pcm, size_t n, bool heard)
{
    ESP_LOGI(TAG, "recorded %u samples, heard=%d", (unsigned)n, heard);
    if (!pcm || n == 0) {
        s_busy = false;
        set_rec_label("REC");
        return;
    }
    set_rec_label("playing");
    audio_play_pcm(session, pcm, n); /* her voice back, unchanged */
}

static void on_done(uint32_t session)
{
    (void)session;
    ESP_LOGI(TAG, "playback done");
    s_busy = false;
    set_rec_label("REC");
}

static void rec_clicked(lv_event_t *e)
{
    (void)e;
    if (s_busy || !audio_available()) {
        return;
    }
    s_busy = true;
    lv_label_set_text(s_rec_label, "listening");
    ESP_LOGI(TAG, "recording");
    audio_record_start(s_session, DIAG_RECORD_S * AUDIO_RATE_HZ);
}

static void screen_touch(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_label_set_text_fmt(s_touch, "touch %d, %d", (int)p.x, (int)p.y);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "touch x=%d y=%d", (int)p.x, (int)p.y);
    }
}

static void on_button(button_id_t id)
{
    if (id == BUTTON_HOME) {
        s_home++;
    } else {
        s_power++;
    }
    ESP_LOGI(TAG, "button %s", id == BUTTON_HOME ? "HOME" : "POWER");
    audio_play(id == BUTTON_HOME ? SND_BLIP_UP : SND_BLIP_DOWN);
    if (display_lock(-1)) {
        lv_label_set_text_fmt(s_buttons, "HOME %lu  POWER %lu", (unsigned long)s_home, (unsigned long)s_power);
        display_unlock();
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    float x, y, z;
    if (!imu_read_g(&x, &y, &z)) {
        lv_label_set_text(s_accel, "IMU not responding");
        return;
    }
    lv_label_set_text_fmt(s_accel, "%.2f  %.2f  %.2f g", (double)x, (double)y, (double)z);
    if (++s_ticks % 5 == 0) {
        ESP_LOGI(TAG, "accel x=%.2f y=%.2f z=%.2f psram free=%u", (double)x, (double)y, (double)z,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
}

static lv_obj_t *label(lv_obj_t *parent, int32_t y, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

void diag_start(void)
{
    if (imu_init() != ESP_OK) {
        ESP_LOGE(TAG, "IMU init failed");
    }
    static const audio_listener_t listener = {.on_recorded = on_recorded, .on_done = on_done};
    s_session = audio_session_begin(&listener);

    display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    s_touch = label(scr, 80, "touch the screen");
    s_accel = label(scr, 130, "IMU ...");
    s_buttons = label(scr, 180, "HOME 0  POWER 0");
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 180, 90);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 250);
    lv_obj_add_event_cb(btn, rec_clicked, LV_EVENT_CLICKED, NULL);
    s_rec_label = lv_label_create(btn);
    lv_obj_set_style_text_font(s_rec_label, &lv_font_montserrat_28, 0);
    lv_label_set_text(s_rec_label, "REC");
    lv_obj_center(s_rec_label);
    lv_obj_t *missing = lv_image_create(scr);
    lv_image_set_src(missing, assets_image("diag/missing")); /* must show the magenta placeholder */
    lv_obj_align(missing, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_event_cb(scr, screen_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, screen_touch, LV_EVENT_PRESSING, NULL);
    lv_timer_create(tick, 200, NULL);
    display_unlock();

    const sensors_listener_t buttons = {.on_button = on_button};
    if (sensors_start(&buttons) != ESP_OK) {
        ESP_LOGE(TAG, "buttons unavailable");
    }
    ESP_LOGI(TAG, "ready");
}
#endif
