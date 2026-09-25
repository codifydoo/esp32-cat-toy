#include <math.h>
#include <stdint.h>
#include "app.h"
#include "audio.h"
#include "cat.h"
#include "tokens.h"
#include "ui_util.h"

/* Meow piano (design: Screen.dc.html piano view, Main.dc.html act('piano'); spec §3.11). Five cats on a
 * ring, one per note of the C major pentatonic scale, so any order sounds nice. */

#define RAD_PER_DEG 0.017453293f

static const struct {
    uint32_t color;
    sound_id_t sound;
    const char *palette; /* exported at scale S (spec §3.11) */
} KEYS[PIANO_KEYS] = {
    {0xFF5A5A, SND_PIANO_C5, "piano_c"}, {0xFF9F3A, SND_PIANO_D5, "piano_d"}, {0xFFD84A, SND_PIANO_E5, "piano_e"},
    {0x5CD67A, SND_PIANO_G5, "piano_g"}, {0x5AA8FF, SND_PIANO_A5, "piano_a"},
};

/* The design's @keyframes bounce (400 ms): time in ms and lift in px, each leg easing out. */
static const int16_t BOUNCE_MS[] = {0, 140, 280, 340, 400};
static const int16_t BOUNCE_Y[] = {0, -22, 0, -6, 0};
#define BOUNCE_KEYS (sizeof(BOUNCE_MS) / sizeof(BOUNCE_MS[0]))

static lv_obj_t *s_note; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_pad[PIANO_KEYS], *s_cat[PIANO_KEYS];
static lv_timer_t *s_sing_timer;
static int s_singing = -1; /* the key singing now, or -1 */

static void bounce_step(void *obj, int32_t t_ms)
{
    int32_t y = 0;
    for (size_t k = 1; k < BOUNCE_KEYS; k++) {
        if (t_ms <= BOUNCE_MS[k]) {
            const int32_t p = (t_ms - BOUNCE_MS[k - 1]) * 1024 / (BOUNCE_MS[k] - BOUNCE_MS[k - 1]);
            const int32_t eased = 1024 - (1024 - p) * (1024 - p) / 1024;
            y = BOUNCE_Y[k - 1] + (BOUNCE_Y[k] - BOUNCE_Y[k - 1]) * eased / 1024;
            break;
        }
    }
    lv_obj_set_style_translate_y(obj, y, 0);
}

/* The singing cat (if any) goes back to happy, its pad empties and the note turns white again. */
static void quiet(void)
{
    if (s_sing_timer) {
        lv_timer_delete(s_sing_timer);
        s_sing_timer = NULL;
    }
    if (s_singing < 0) {
        return;
    }
    const int i = s_singing;
    s_singing = -1;
    lv_anim_delete(s_cat[i], bounce_step);
    lv_obj_set_style_translate_y(s_cat[i], 0, 0);
    cat_set_state(s_cat[i], CAT_HAPPY);
    lv_obj_set_style_bg_opa(s_pad[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_recolor_opa(s_note, LV_OPA_TRANSP, 0);
}

static void sing_done(lv_timer_t *t)
{
    (void)t;
    s_sing_timer = NULL; /* one-shot timers delete themselves */
    if (!s_note) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    quiet();
}

static void key_tapped(lv_event_t *e)
{
    if (!s_note) {
        return;
    }
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    quiet(); /* a new tap replaces the last song (spec §3.11) */
    s_singing = i;
    cat_set_state(s_cat[i], CAT_SINGING);
    lv_obj_set_style_bg_opa(s_pad[i], PIANO_PAD_FILL_OPA, 0);
    lv_obj_set_style_image_recolor(s_note, lv_color_hex(KEYS[i].color), 0);
    lv_obj_set_style_image_recolor_opa(s_note, LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_cat[i]);
    lv_anim_set_exec_cb(&a, bounce_step);
    lv_anim_set_values(&a, 0, BOUNCE_MS[BOUNCE_KEYS - 1]);
    lv_anim_set_duration(&a, (uint32_t)BOUNCE_MS[BOUNCE_KEYS - 1]);
    lv_anim_start(&a);
    audio_play(KEYS[i].sound); /* one voice: this cuts off a meow still playing */
    s_sing_timer = lv_timer_create(sing_done, PIANO_SING_MS, NULL);
    lv_timer_set_repeat_count(s_sing_timer, 1);
}

static void piano_create(lv_obj_t *screen)
{
    s_singing = -1;
    s_note = ui_image(screen, "ui/piano_note", PIANO_NOTE_X, PIANO_NOTE_Y);
    for (int i = 0; i < PIANO_KEYS; i++) {
        const float a = (-90.0f + 72.0f * (float)i) * RAD_PER_DEG; /* C at the top, then clockwise */
        const int32_t cx = SCREEN_CENTER + (int32_t)lroundf(PIANO_RING_R * cosf(a));
        const int32_t cy = SCREEN_CENTER + (int32_t)lroundf(PIANO_RING_R * sinf(a));
        lv_obj_t *pad = ui_circle(screen, cx, cy, PIANO_PAD_SIZE, KEYS[i].color);
        lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(pad, PIANO_PAD_BORDER, 0);
        lv_obj_set_style_border_color(pad, lv_color_hex(KEYS[i].color), 0);
        lv_obj_set_style_border_opa(pad, PIANO_PAD_LINE_OPA, 0);
        lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(pad, (PIANO_TAP_SIZE - PIANO_PAD_SIZE + 1) / 2); /* the 150 px target */
        lv_obj_add_event_cb(pad, key_tapped, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_pad[i] = pad;
        s_cat[i] = cat_create_colored(screen, CAT_SCALE_S, KEYS[i].palette); /* not clickable: taps reach the pad */
        cat_set_origin(s_cat[i], cx - PIANO_CAT_DX, cy - PIANO_CAT_DY);
    }
}

static void piano_destroy(void)
{
    if (s_sing_timer) {
        lv_timer_delete(s_sing_timer);
        s_sing_timer = NULL;
    }
    for (int i = 0; i < PIANO_KEYS; i++) {
        lv_anim_delete(s_cat[i], bounce_step);
        s_cat[i] = NULL;
        s_pad[i] = NULL;
    }
    s_singing = -1;
    s_note = NULL;
    /* the meow: app_manager calls audio_stop_all() after this */
}

const app_t app_piano = {
    .id = "piano",
    .icon = "apps/piano",
    .icon_small = "apps/piano_96",
    .color = COLOR_PIANO,
    .name_sound = SND_NAME_PIANO,
    .swipe_sound = SND_SWIPE_PIANO,
    .react = CAT_LAUGHING,
    .create = piano_create,
    .destroy = piano_destroy,
};
