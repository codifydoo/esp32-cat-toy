#include <math.h>
#include <stdint.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "display.h"
#include "envelope.h"
#include "esp_heap_caps.h"
#include "tokens.h"
#include "ui_util.h"
#include "voice_fx.h"

typedef enum { V_IDLE, V_LISTEN, V_PLAY } vstate_t;

static const uint32_t ACCENT[VOICE_COUNT] = {COLOR_COSTUME_MOUSE, COLOR_COSTUME_LION, COLOR_COSTUME_ROBOT,
                                             COLOR_COSTUME_CAVE};
static const cat_costume_t COSTUME[VOICE_COUNT] = {CAT_COSTUME_MOUSE, CAT_COSTUME_LION, CAT_COSTUME_ROBOT,
                                                   CAT_COSTUME_MINER};
static const sound_id_t STING[VOICE_COUNT] = {SND_COSTUME_MOUSE, SND_COSTUME_LION, SND_COSTUME_ROBOT,
                                              SND_COSTUME_CAVE};
/* Playback waves: two arcs on each side of the cat (lv_arc angles: 0 = 3 o'clock, clockwise). */
static const struct {
    int16_t r, start, end;
} WAVES[4] = {{84, 330, 30}, {110, 325, 35}, {84, 150, 210}, {110, 145, 215}};

static vstate_t s_state;
static voice_t s_voice;
static uint32_t s_session; /* 0 while the app is closed; read by audio callbacks under the LVGL lock */
static lv_obj_t *s_cat, *s_cave, *s_mic, *s_glyph, *s_pulse, *s_countdown;
static lv_obj_t *s_ring[VOICES_RING_DOTS];
static lv_obj_t *s_waves[4];
static lv_timer_t *s_start_timer;
static lv_timer_t *s_mouth_timer;
static uint16_t s_level_peak; /* audio task only */
static uint8_t s_level_calls; /* audio task only */
/* Published by on_level (audio task) without the LVGL lock, consumed by mouth_tick (LVGL task).
 * A stale publish after the session ends is harmless: the timer is gone, or s_state != V_PLAY. */
static volatile uint8_t s_mouth_frame;
static volatile bool s_mouth_pending;

static void set_opa(void *obj, int32_t v) { lv_obj_set_style_opa(obj, (lv_opa_t)v, 0); }
static void set_countdown(void *arc, int32_t v) { lv_arc_set_value(arc, (int16_t)v); }

static void loop_opa(lv_obj_t *obj, int32_t from, int32_t to, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, set_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void show_state(vstate_t st)
{
    s_state = st;
    const bool listen = st == V_LISTEN, play = st == V_PLAY, off = !audio_available();
    uint32_t fill = off ? COLOR_MIC_OFF : listen ? COLOR_MIC_LISTEN : play ? COLOR_MIC_PLAY : COLOR_MIC_IDLE;
    lv_obj_set_style_bg_color(s_mic, lv_color_hex(fill), 0);
    lv_obj_set_style_border_color(s_mic, lv_color_hex(listen ? COLOR_MIC_RING_LISTEN : COLOR_MIC_RING), 0);
    lv_obj_set_style_image_opa(s_glyph, play || off ? 102 : LV_OPA_COVER, 0);

    lv_anim_delete(s_pulse, set_opa);
    lv_anim_delete(s_countdown, set_countdown);
    ui_set_hidden(s_pulse, !listen);
    ui_set_hidden(s_countdown, !listen);
    if (listen) {
        loop_opa(s_pulse, LV_OPA_COVER, LV_OPA_TRANSP, 1000);
        lv_arc_set_value(s_countdown, 1000);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_countdown);
        lv_anim_set_exec_cb(&a, set_countdown);
        lv_anim_set_values(&a, 1000, 0);
        lv_anim_set_duration(&a, VOICES_RECORD_MAX_S * 1000 + VOICES_BLIP_LEAD_MS);
        lv_anim_start(&a);
    }
    for (int i = 0; i < 4; i++) {
        lv_anim_delete(s_waves[i], set_opa);
        lv_obj_set_style_opa(s_waves[i], LV_OPA_COVER, 0);
        ui_set_hidden(s_waves[i], !play);
        if (play) {
            loop_opa(s_waves[i], LV_OPA_COVER, 60, 600);
        }
    }
    cat_set_state(s_cat, listen ? CAT_LISTENING : play ? CAT_TALKING : CAT_HAPPY);
}

static void apply_costume(void)
{
    cat_set_costume(s_cat, COSTUME[s_voice]);
    lv_color_t accent = lv_color_hex(ACCENT[s_voice]);
    for (int i = 0; i < VOICES_RING_DOTS; i++) {
        lv_obj_set_style_bg_color(s_ring[i], accent, 0);
    }
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_arc_color(s_waves[i], accent, LV_PART_MAIN);
    }
    ui_set_hidden(s_cave, s_voice != VOICE_CAVE);
}

/* ---- audio-task callbacks: take the LVGL lock, then check the app is still open for this session ---- */

static bool lock_session(uint32_t session)
{
    if (!display_lock(-1)) {
        return false;
    }
    if (session == 0 || session != s_session) {
        display_unlock();
        return false;
    }
    return true;
}

static void on_recorded(uint32_t session, int16_t *pcm, size_t n, bool heard)
{
    if (!pcm || n == 0 || !heard) {
        heap_caps_free(pcm);
        if (lock_session(session)) {
            show_state(V_IDLE);
            audio_play(SND_MEOW); /* nothing heard: a meow instead of playing silence */
            display_unlock();
        }
        return;
    }
    const voice_t v = s_voice;
    size_t out_n = voice_fx_output_len(v, n, AUDIO_RATE_HZ);
    int16_t *out = heap_caps_malloc(out_n * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (out) {
        out_n = voice_fx_apply(v, pcm, n, out, out_n, AUDIO_RATE_HZ);
    }
    heap_caps_free(pcm);
    if (!lock_session(session)) {
        heap_caps_free(out);
        return;
    }
    show_state(out ? V_PLAY : V_IDLE);
    s_level_calls = 0;
    s_level_peak = 0;
    display_unlock();
    if (out) {
        audio_play_pcm(session, out, out_n); /* ownership passes to the audio service */
    }
}

static void on_level(uint32_t session, uint16_t peak)
{
    (void)session; /* a stale publish is harmless: see s_mouth_pending above */
    if (peak > s_level_peak) {
        s_level_peak = peak;
    }
    if (++s_level_calls < 6) { /* 6 x 20 ms = 120 ms, about 8 fps */
        return;
    }
    /* No LVGL/display_lock here: the LVGL task can hold the lock for a whole render+flush, and a
     * missed try-lock would both log an error and drop this frame. mouth_tick() on the LVGL task
     * picks the published frame up instead. */
    s_mouth_frame = envelope_mouth(s_level_peak);
    s_mouth_pending = true;
    s_level_calls = 0;
    s_level_peak = 0;
}

static void on_done(uint32_t session)
{
    if (lock_session(session)) {
        if (s_state == V_PLAY) {
            show_state(V_IDLE);
        }
        display_unlock();
    }
}

/* ---- touch handlers and timers (LVGL task) ---- */

static void start_recording(lv_timer_t *t)
{
    (void)t;
    s_start_timer = NULL;
    audio_record_start(s_session, (size_t)VOICES_RECORD_MAX_S * AUDIO_RATE_HZ);
}

static void mouth_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    if (s_state == V_PLAY && s_mouth_pending) {
        s_mouth_pending = false;
        cat_set_mouth(s_cat, s_mouth_frame);
    }
}

static void mic_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    if (!audio_available()) {
        return;
    }
    if (s_state == V_IDLE) {
        audio_play(SND_BLIP_UP);
        show_state(V_LISTEN);
        s_start_timer = lv_timer_create(start_recording, VOICES_BLIP_LEAD_MS, NULL);
        lv_timer_set_repeat_count(s_start_timer, 1);
    } else if (s_state == V_LISTEN) {
        if (s_start_timer) { /* tapped again before recording began */
            lv_timer_delete(s_start_timer);
            s_start_timer = NULL;
            show_state(V_IDLE);
        } else {
            audio_record_stop(s_session);
        }
    }
}

static void cat_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    if (s_state != V_IDLE) {
        return;
    }
    s_voice = (voice_t)((s_voice + 1) % VOICE_COUNT);
    apply_costume();
    audio_play(STING[s_voice]);
}

/* ---- screen ---- */

static lv_obj_t *wave(lv_obj_t *scr, int16_t r, int16_t start, int16_t end)
{
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, r * 2, r * 2);
    lv_obj_set_pos(arc, VOICES_RING_CX - r, VOICES_WAVE_CY - r);
    lv_arc_set_bg_angles(arc, start, end);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    return arc;
}

static void build(lv_obj_t *scr)
{
    int16_t x, y;
    assets_ui_pos("ui/cave_arch", &x, &y);
    s_cave = ui_image(scr, "ui/cave_arch", x, y);

    for (int k = 0; k < VOICES_RING_DOTS; k++) {
        float a = 2.0f * 3.14159265f * (float)k / VOICES_RING_DOTS - 3.14159265f / 2.0f;
        s_ring[k] = ui_circle(scr, VOICES_RING_CX + (int32_t)lroundf(VOICES_RING_R * cosf(a)),
                              VOICES_RING_CY + (int32_t)lroundf(VOICES_RING_R * sinf(a)), VOICES_RING_DOT, 0xFFFFFF);
    }

    s_cat = cat_create(scr, CAT_SCALE_M);
    cat_set_origin(s_cat, VOICES_CAT_ORIGIN_X, VOICES_CAT_ORIGIN_Y);
    for (int i = 0; i < 4; i++) {
        s_waves[i] = wave(scr, WAVES[i].r, WAVES[i].start, WAVES[i].end);
    }

    lv_obj_t *hit = ui_circle(scr, VOICES_CAT_HIT_X + VOICES_CAT_HIT_SIZE / 2, VOICES_CAT_HIT_Y + VOICES_CAT_HIT_SIZE / 2,
                              VOICES_CAT_HIT_SIZE, 0);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, cat_tapped, LV_EVENT_CLICKED, NULL);

    const int32_t mic_cx = VOICES_MIC_X + VOICES_MIC_SIZE / 2, mic_cy = VOICES_MIC_Y + VOICES_MIC_SIZE / 2;
    s_pulse = ui_circle(scr, mic_cx, mic_cy, 200, 0);
    lv_obj_set_style_bg_opa(s_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pulse, 6, 0);
    lv_obj_set_style_border_color(s_pulse, lv_color_hex(COLOR_MIC_LISTEN), 0);

    s_mic = ui_circle(scr, mic_cx, mic_cy, 188, COLOR_MIC_IDLE);
    lv_obj_set_style_border_width(s_mic, 10, 0);
    lv_obj_add_flag(s_mic, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_mic, 16); /* the whole 220 px button */
    lv_obj_add_event_cb(s_mic, mic_tapped, LV_EVENT_CLICKED, NULL);

    s_countdown = lv_arc_create(scr);
    lv_obj_remove_style_all(s_countdown);
    lv_obj_remove_flag(s_countdown, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_countdown, 208, 208);
    lv_obj_set_pos(s_countdown, mic_cx - 104, mic_cy - 104);
    lv_arc_set_rotation(s_countdown, 270);
    lv_arc_set_bg_angles(s_countdown, 0, 360);
    lv_arc_set_range(s_countdown, 0, 1000);
    lv_obj_set_style_arc_width(s_countdown, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_countdown, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_countdown, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_countdown, LV_OPA_COVER, LV_PART_INDICATOR);

    assets_ui_pos("ui/mic_glyph", &x, &y);
    s_glyph = ui_image(scr, "ui/mic_glyph", VOICES_MIC_X + x, VOICES_MIC_Y + y); /* not clickable: taps reach the mic */
}

static void voices_create(lv_obj_t *screen)
{
    static const audio_listener_t listener = {.on_recorded = on_recorded, .on_level = on_level, .on_done = on_done};
    s_voice = VOICE_MOUSE;
    build(screen);
    apply_costume();
    show_state(V_IDLE);
    s_mouth_pending = false;
    s_mouth_timer = lv_timer_create(mouth_tick, 40, NULL);
    s_session = audio_session_begin(&listener);
}

static void voices_destroy(void)
{
    uint32_t session = s_session;
    s_session = 0; /* callbacks re-check this under the LVGL lock, which we hold */
    audio_session_end(session);
    if (s_start_timer) {
        lv_timer_delete(s_start_timer);
        s_start_timer = NULL;
    }
    if (s_mouth_timer) {
        lv_timer_delete(s_mouth_timer);
        s_mouth_timer = NULL;
    }
    s_cat = NULL;
}

const app_t app_voices = {
    .id = "voices",
    .icon = "apps/voices",
    .icon_small = "apps/voices_96",
    .color = COLOR_VOICES,
    .name_sound = SND_NAME_VOICES,
    .swipe_sound = SND_SWIPE_VOICES,
    .react = CAT_TALKING,
    .create = voices_create,
    .destroy = voices_destroy,
};
