#include "cat.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "assets.h"
#include "assets_manifest.h"
#include "esp_random.h"

#define FRAME_MS        83  /* 12 fps */
#define TALK_HOLD_MS    250 /* after cat_set_mouth, auto-talk waits this long */
#define PAW_DRAWN_X     100 /* where the exporter drew the paw part (design px) */
#define PAW_DRAWN_Y     172
#define EYE_SPACING     56  /* right eye is 56 design px right of the left eye */
#define LOOK_RANGE      4   /* pupils move up to 4 design px */

typedef enum { EARS_NORMAL, EARS_PERK, EARS_DROOP } ears_t;
typedef enum { EYES_OPEN, EYES_HAPPY, EYES_BIG, EYES_HALF, EYES_CLOSED, EYES_SPIRAL } eyes_t;
typedef enum { PAWS_DOWN, PAWS_UP, PAWS_HOLD, PAWS_MOUTH, PAWS_BROOM } paws_t;
typedef enum { FX_NONE, FX_WAVES, FX_Z1, FX_ZZ, FX_STARS, FX_SPARKLES, FX_PURR, FX_NOTE, FX_SHH, FX_FISH, FX_BROOM } fx_t;

typedef struct {
    ears_t ears;
    eyes_t eyes;
    const char *mouth; /* NULL: talking mouth frames */
    bool blush;
    paws_t paws;
    fx_t fx;
    int8_t look_x10, look_y10; /* default pupil offset x10 */
} expr_t;

static const expr_t EXPR[CAT_STATE_COUNT] = {
    [CAT_HAPPY] = {EARS_NORMAL, EYES_OPEN, "mouth_w", true, PAWS_DOWN, FX_NONE, 0, 0},
    [CAT_LAUGHING] = {EARS_NORMAL, EYES_HAPPY, "mouth_open", true, PAWS_DOWN, FX_NONE, 0, 0},
    [CAT_SURPRISED] = {EARS_PERK, EYES_BIG, "mouth_o", false, PAWS_DOWN, FX_NONE, 0, 0},
    [CAT_LISTENING] = {EARS_PERK, EYES_OPEN, "mouth_small", false, PAWS_DOWN, FX_WAVES, 7, -7},
    [CAT_TALKING] = {EARS_NORMAL, EYES_OPEN, NULL, true, PAWS_DOWN, FX_NONE, 0, 0},
    [CAT_SLEEPY] = {EARS_DROOP, EYES_HALF, "mouth_sleepy", false, PAWS_DOWN, FX_Z1, 0, 0},
    [CAT_SLEEPING] = {EARS_DROOP, EYES_CLOSED, "mouth_smallw", false, PAWS_DOWN, FX_ZZ, 0, 0},
    [CAT_DIZZY] = {EARS_DROOP, EYES_SPIRAL, "mouth_wavy", false, PAWS_DOWN, FX_STARS, 0, 0},
    [CAT_EATING] = {EARS_NORMAL, EYES_HAPPY, "mouth_munch", true, PAWS_HOLD, FX_FISH, 0, 0},
    [CAT_PROUD] = {EARS_PERK, EYES_HAPPY, "mouth_open", true, PAWS_UP, FX_SPARKLES, 0, 0},
    [CAT_YAWN] = {EARS_DROOP, EYES_CLOSED, "mouth_yawn", false, PAWS_UP, FX_NONE, 0, 0},
    [CAT_PURRING] = {EARS_NORMAL, EYES_HAPPY, "mouth_w", true, PAWS_DOWN, FX_PURR, 0, 0},
    [CAT_SINGING] = {EARS_PERK, EYES_HAPPY, NULL, true, PAWS_DOWN, FX_NOTE, 0, 0},
    [CAT_SHH] = {EARS_DROOP, EYES_HALF, "mouth_osmall", false, PAWS_MOUTH, FX_SHH, 0, 0},
    [CAT_SWEEP] = {EARS_NORMAL, EYES_HAPPY, "mouth_w", true, PAWS_BROOM, FX_BROOM, 0, 0},
};

static const char *const STATE_NAME[CAT_STATE_COUNT] = {
    "happy", "laughing", "surprised", "listening", "talking", "sleepy", "sleeping", "dizzy",
    "eating", "proud", "yawn", "purring", "singing", "shh", "sweep",
};
static const char *const EARS_PART[] = {"ears_normal", "ears_perk", "ears_droop"};
static const char *const TAIL_PART[] = {"tail_f0", "tail_f1", "tail_f2", "tail_f1"};
static const char *const BLINK_PART[] = {"eyes_open", "eyes_half", "eyes_closed", "eyes_half"};
static const char *const SPIRAL_PART[] = {"eyes_spiral_f0", "eyes_spiral_f1", "eyes_spiral_f2", "eyes_spiral_f3"};
static const char *const TALK_PART[] = {"mouth_w", "mouth_talk_b", "mouth_talk_c"};
static const uint8_t TALK_SEQ[] = {0, 1, 2, 1};
static const char *const MANE_PART[] = {"lion_mane_f0", "lion_mane_f1"};
static const char *const ROBOT_PART[] = {"robot_helmet_f0", "robot_helmet_f1"};
static const char *const MINER_PART[] = {"miner_hat_f0", "miner_hat_f1"};
static const char *const PURR_PART[] = {"fx_purr_f0", "fx_purr_f1"};
static const char *const ZZ_PART[] = {"fx_zz_f1", "fx_zz_f2"};
static const int8_t BOB[] = {0, -1, -2, -3, -2, -1};
static const int16_t PAW_POS[][4] = {
    [PAWS_DOWN] = {76, 172, 124, 172},
    [PAWS_UP] = {20, 76, 180, 76},
    [PAWS_HOLD] = {58, 170, 142, 170},
    [PAWS_MOUTH] = {100, 138, 124, 172},
    [PAWS_BROOM] = {138, 132, 158, 76},
};
static const float SCALE_F[CAT_SCALE_COUNT] = {0.65f, 1.0f, 1.3f};
static const char SCALE_C[CAT_SCALE_COUNT] = {'S', 'M', 'L'};
static const char *const VARIANT_NAME[CAT_VARIANT_COUNT] = {"orange", "grey", "tux"};

typedef enum {
    L_MANE, L_TAIL, L_BODY, L_EARS, L_COSTUME_EARS, L_HEAD, L_BLUSH, L_EYES, L_PUPIL_L, L_PUPIL_R,
    L_HL_L, L_HL_R, L_MOUTH, L_PROP, L_PAW_A, L_PAW_B, L_HELMET, L_FX, L_COUNT
} layer_t;

typedef struct {
    lv_obj_t *root, *rig;
    lv_obj_t *img[L_COUNT];
    const char *shown[L_COUNT];
    cat_scale_t scale;
    const char *palette; /* NULL: the chosen variant (cat_set_variant) */
    cat_state_t state;
    cat_costume_t costume;
    float look_x, look_y;
    bool look_set;
    uint8_t talk_frame;
    uint32_t talk_ext_ms;
    uint32_t tick, next_blink;
    uint8_t blink; /* 0 open, 1..3 = half, closed, half */
    lv_timer_t *timer;
} cat_t;

static cat_variant_t s_variant = CAT_VARIANT_ORANGE;

static const cat_part_pos_t *find_part(const char *part)
{
    for (size_t i = 0; i < CAT_PARTS_COUNT; i++) {
        if (strcmp(CAT_PARTS[i].part, part) == 0) {
            return &CAT_PARTS[i];
        }
    }
    return NULL;
}

static void asset_name(char *out, size_t n, cat_scale_t scale, const char *palette, const char *part)
{
    const cat_part_pos_t *p = find_part(part);
    const char *owner = p && p->shared ? "shared" : palette ? palette : VARIANT_NAME[s_variant];
    snprintf(out, n, "cat/%s/%c/%s", owner, SCALE_C[scale], part);
}

void cat_part_asset(char *out, size_t n, cat_scale_t scale, const char *part)
{
    asset_name(out, n, scale, NULL, part);
}

static int16_t sc(const cat_t *c, float design_px) { return (int16_t)lroundf(design_px * SCALE_F[c->scale]); }

/* Shows `part` on layer `l` at its exported position plus (dx, dy); NULL hides the layer. */
static void place(cat_t *c, layer_t l, const char *part, int16_t dx, int16_t dy)
{
    lv_obj_t *img = c->img[l];
    if (!part) {
        if (c->shown[l]) {
            lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
            c->shown[l] = NULL;
        }
        return;
    }
    if (!c->shown[l] || strcmp(c->shown[l], part) != 0) {
        char name[64];
        asset_name(name, sizeof(name), c->scale, c->palette, part);
        lv_image_set_src(img, assets_image(name));
        lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
        c->shown[l] = part;
    }
    const cat_part_pos_t *p = find_part(part);
    lv_obj_set_pos(img, (p ? p->x[c->scale] : 0) + dx, (p ? p->y[c->scale] : 0) + dy);
}

static void apply(cat_t *c)
{
    const expr_t *e = &EXPR[c->state];
    const uint32_t t = c->tick;
    const bool talking = c->state == CAT_TALKING || c->state == CAT_SINGING;

    place(c, L_MANE, c->costume == CAT_COSTUME_LION ? MANE_PART[talking ? (t / 3) % 2 : 0] : NULL, 0, 0);
    place(c, L_TAIL, TAIL_PART[(t / 2) % 4], 0, 0);
    place(c, L_BODY, "body", 0, 0);
    bool own_ears = c->costume == CAT_COSTUME_NONE || c->costume == CAT_COSTUME_MINER;
    place(c, L_EARS, own_ears ? EARS_PART[e->ears] : NULL, 0, 0);
    place(c, L_COSTUME_EARS,
          c->costume == CAT_COSTUME_MOUSE ? "mouse_ears" : c->costume == CAT_COSTUME_LION ? "lion_ears" : NULL, 0, 0);
    place(c, L_HEAD, "head", 0, 0);
    place(c, L_BLUSH, e->blush ? "blush" : NULL, 0, 0);

    const char *eyes;
    bool pupils = false, big = false;
    switch (e->eyes) {
    case EYES_OPEN:
        eyes = BLINK_PART[c->blink];
        pupils = c->blink == 0;
        break;
    case EYES_HAPPY: eyes = "eyes_happy"; break;
    case EYES_BIG:
        eyes = "eyes_big"; /* irises only: the big pupils follow cat_set_look like the open ones (design) */
        pupils = big = true;
        break;
    case EYES_HALF: eyes = "eyes_half"; break;
    case EYES_CLOSED: eyes = "eyes_closed"; break;
    default: eyes = SPIRAL_PART[t % 4]; break;
    }
    place(c, L_EYES, eyes, 0, 0);
    float lx = c->look_set ? c->look_x : e->look_x10 / 10.0f;
    float ly = c->look_set ? c->look_y : e->look_y10 / 10.0f;
    int16_t ox = sc(c, lx * LOOK_RANGE), oy = sc(c, ly * LOOK_RANGE), gap = sc(c, EYE_SPACING);
    const char *pupil = big ? "pupil_big" : "pupil", *hl = big ? "highlight_big" : "highlight";
    place(c, L_PUPIL_L, pupils ? pupil : NULL, ox, oy);
    place(c, L_PUPIL_R, pupils ? pupil : NULL, (int16_t)(gap + ox), oy);
    place(c, L_HL_L, pupils ? hl : NULL, ox, oy);
    place(c, L_HL_R, pupils ? hl : NULL, (int16_t)(gap + ox), oy);

    const char *mouth = e->mouth;
    if (talking) {
        if (lv_tick_elaps(c->talk_ext_ms) > TALK_HOLD_MS) {
            c->talk_frame = TALK_SEQ[(t / 2) % 4];
        }
        mouth = TALK_PART[c->talk_frame];
    } else if (c->state == CAT_EATING) {
        mouth = (t / 2) % 2 ? "mouth_smallw" : "mouth_munch";
    }
    place(c, L_MOUTH, mouth, 0, 0);
    place(c, L_PROP, e->fx == FX_FISH ? "prop_fish" : e->fx == FX_BROOM ? "prop_broom" : NULL, 0, 0);

    const int16_t *pp = PAW_POS[e->paws];
    place(c, L_PAW_A, "paw", sc(c, pp[0] - PAW_DRAWN_X), sc(c, pp[1] - PAW_DRAWN_Y));
    place(c, L_PAW_B, "paw", sc(c, pp[2] - PAW_DRAWN_X), sc(c, pp[3] - PAW_DRAWN_Y));
    place(c, L_HELMET,
          c->costume == CAT_COSTUME_ROBOT   ? ROBOT_PART[(t / 6) % 2]
          : c->costume == CAT_COSTUME_MINER ? MINER_PART[(t / 6) % 2]
                                            : NULL,
          0, 0);

    const char *fx = NULL;
    switch (e->fx) {
    case FX_WAVES: fx = "fx_waves"; break;
    case FX_Z1: fx = "fx_zz_f0"; break;
    case FX_ZZ: fx = ZZ_PART[(t / 4) % 2]; break;
    case FX_STARS: fx = "fx_stars"; break;
    case FX_SPARKLES: fx = (t / 3) % 2 ? "fx_sparkles" : NULL; break;
    case FX_PURR: fx = PURR_PART[(t / 2) % 2]; break;
    case FX_NOTE: fx = "fx_note"; break;
    case FX_SHH: fx = "fx_shh"; break;
    default: break;
    }
    place(c, L_FX, fx, 0, 0);

    bool calm = c->state == CAT_SLEEPING || c->state == CAT_SLEEPY || c->state == CAT_SHH;
    lv_obj_set_style_translate_y(c->rig, calm ? 0 : sc(c, BOB[(t / 2) % 6]), 0);
    /* Spec §3.4 fallback: rotating the whole rig measured 7 FPS on the device, so dizzy wobbles sideways. */
    lv_obj_set_style_translate_x(c->rig, c->state == CAT_DIZZY ? ((t / 4) % 2 ? sc(c, 10) : sc(c, -10)) : 0, 0);
}

static void on_timer(lv_timer_t *timer)
{
    cat_t *c = lv_timer_get_user_data(timer);
    c->tick++;
    if (c->blink) {
        c->blink = (uint8_t)((c->blink + 1) % 4);
    } else if (c->tick >= c->next_blink) {
        c->blink = 1;
        c->next_blink = c->tick + 36 + esp_random() % 37; /* every 3 to 6 s */
    }
    apply(c);
}

static void on_delete(lv_event_t *e)
{
    cat_t *c = lv_event_get_user_data(e);
    lv_timer_delete(c->timer);
    lv_free(c);
}

static cat_t *get(lv_obj_t *obj) { return (cat_t *)lv_obj_get_user_data(obj); }

void cat_set_variant(cat_variant_t variant) { s_variant = variant < CAT_VARIANT_COUNT ? variant : CAT_VARIANT_ORANGE; }

lv_obj_t *cat_create_colored(lv_obj_t *parent, cat_scale_t scale, const char *palette)
{
    cat_t *c = lv_malloc_zeroed(sizeof(cat_t));
    LV_ASSERT_MALLOC(c);
    c->scale = scale;
    c->palette = palette;
    c->state = CAT_HAPPY;
    c->next_blink = 40;
    const int32_t w = CAT_CANVAS_W[scale], h = CAT_CANVAS_H[scale];

    c->root = lv_obj_create(parent);
    lv_obj_remove_style_all(c->root);
    lv_obj_set_size(c->root, w, h);
    lv_obj_remove_flag(c->root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    c->rig = lv_obj_create(c->root);
    lv_obj_remove_style_all(c->rig);
    lv_obj_set_size(c->rig, w, h);
    lv_obj_remove_flag(c->rig, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_transform_pivot_x(c->rig, w / 2, 0);
    lv_obj_set_style_transform_pivot_y(c->rig, h / 2, 0);

    for (int l = 0; l < L_COUNT; l++) {
        c->img[l] = lv_image_create(c->rig);
        lv_obj_remove_flag(c->img[l], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(c->img[l], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_user_data(c->root, c);
    lv_obj_add_event_cb(c->root, on_delete, LV_EVENT_DELETE, c);
    apply(c);
    c->timer = lv_timer_create(on_timer, FRAME_MS, c);
    return c->root;
}

lv_obj_t *cat_create(lv_obj_t *parent, cat_scale_t scale) { return cat_create_colored(parent, scale, NULL); }

void cat_set_origin(lv_obj_t *cat, int32_t x, int32_t y)
{
    cat_t *c = get(cat);
    lv_obj_set_pos(cat, x - CAT_ORIGIN_PX[c->scale], y - CAT_ORIGIN_PX[c->scale]);
}

void cat_set_state(lv_obj_t *cat, cat_state_t state)
{
    cat_t *c = get(cat);
    c->state = state < CAT_STATE_COUNT ? state : CAT_HAPPY;
    c->look_set = false;
    apply(c);
}

void cat_set_costume(lv_obj_t *cat, cat_costume_t costume)
{
    cat_t *c = get(cat);
    c->costume = costume < CAT_COSTUME_COUNT ? costume : CAT_COSTUME_NONE;
    apply(c);
}

void cat_set_look(lv_obj_t *cat, float x, float y)
{
    cat_t *c = get(cat);
    c->look_x = x < -1 ? -1 : x > 1 ? 1 : x;
    c->look_y = y < -1 ? -1 : y > 1 ? 1 : y;
    c->look_set = true;
    apply(c);
}

void cat_set_mouth(lv_obj_t *cat, uint8_t frame)
{
    cat_t *c = get(cat);
    c->talk_frame = frame > 2 ? 2 : frame;
    c->talk_ext_ms = lv_tick_get();
    apply(c);
}

const char *cat_state_name(cat_state_t state) { return state < CAT_STATE_COUNT ? STATE_NAME[state] : "?"; }
