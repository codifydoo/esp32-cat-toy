#include "scenes.h"
#include <math.h>
#include "assets.h"
#include "cat.h"
#include "ui_util.h"

#define DEG_TO_RAD (3.14159265f / 180.0f)

/* Design nightStars: x, y, and the twinkle's delay in ms. */
static const int16_t NIGHT_STAR_POS[NIGHT_STARS][3] = {
    {300, 70, 0},     {360, 120, 800}, {390, 196, 1600}, {252, 44, 400},  {196, 96, 1200},
    {96, 200, 2000},  {70, 268, 600},  {380, 276, 1400}, {170, 150, 2400},
};

static lv_obj_t *layer(lv_obj_t *parent)
{
    lv_obj_t *o = ui_plain(parent);
    lv_obj_set_size(o, SCREEN_SIZE, SCREEN_SIZE);
    return o;
}

static void image_opa(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }

/* The design's .twinkle: 30 % -> 100 % -> 30 % over 3 s, forever, starting after delay_ms. */
static void twinkle(lv_obj_t *img, uint32_t delay_ms)
{
    lv_obj_set_style_image_opa(img, NIGHT_TWINKLE_LOW_OPA, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, image_opa);
    lv_anim_set_values(&a, NIGHT_TWINKLE_LOW_OPA, LV_OPA_COVER);
    lv_anim_set_duration(&a, NIGHT_TWINKLE_MS / 2);
    lv_anim_set_reverse_duration(&a, NIGHT_TWINKLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_start(&a);
}

void scene_night_create(night_scene_t *s, lv_obj_t *parent)
{
    s->root = layer(parent);
    ui_circle(s->root, NIGHT_MOON_CX, NIGHT_MOON_CY, NIGHT_MOON_D, COLOR_MOON);
    ui_circle(s->root, NIGHT_MOON_CUT_CX, NIGHT_MOON_CUT_CY, NIGHT_MOON_CUT_D, 0x000000);
    for (size_t i = 0; i < NIGHT_STARS; i++) {
        lv_obj_t *star = ui_image(s->root, "ui/star", NIGHT_STAR_POS[i][0], NIGHT_STAR_POS[i][1]);
        twinkle(star, (uint32_t)NIGHT_STAR_POS[i][2]);
    }
    s->cat = cat_create(s->root, CAT_SCALE_M);
    cat_set_origin(s->cat, NIGHT_CAT_X, NIGHT_CAT_Y);
    cat_set_state(s->cat, CAT_SLEEPING);
    for (int i = 0; i < HOUR_STARS; i++) {
        /* Design: star i at 90° + (i - 2) * 12° on r 200, the first one on the left. */
        const float a = (90.0f + (float)((i - HOUR_STARS / 2) * HOUR_STAR_STEP_DEG)) * DEG_TO_RAD;
        const int32_t x = SCREEN_CENTER - (int32_t)lroundf(HOUR_STAR_R * cosf(a)) - HOUR_STAR_SIZE / 2;
        const int32_t y = SCREEN_CENTER + (int32_t)lroundf(HOUR_STAR_R * sinf(a)) - HOUR_STAR_SIZE / 2;
        s->hour[i] = ui_image(s->root, "ui/hour_star_off", x, y);
    }
    s->lit = 0;
}

void scene_night_set_stars(night_scene_t *s, uint8_t lit)
{
    if (lit > HOUR_STARS) {
        lit = HOUR_STARS;
    }
    if (lit == s->lit) {
        return;
    }
    s->lit = lit;
    for (int i = 0; i < HOUR_STARS; i++) {
        lv_image_set_src(s->hour[i], assets_image(i < lit ? "ui/hour_star_on" : "ui/hour_star_off"));
    }
}

/* Turns the rays: v is the angle in 0.1°, over one ray's 30° (the pattern repeats every 30°). */
static void rays_step(lv_anim_t *a, int32_t v)
{
    day_scene_t *s = lv_anim_get_user_data(a);
    lv_obj_t *rays = a->var;
    const float c = DAY_SUN_BOX / 2.0f;
    for (uint32_t i = 0; i < DAY_RAYS; i++) {
        const float ang = ((float)i * (360.0f / DAY_RAYS) + (float)v / 10.0f) * DEG_TO_RAD;
        const float co = cosf(ang), si = sinf(ang);
        s->rays[i][0].x = (lv_value_precise_t)lroundf(c + DAY_RAY_R0 * co);
        s->rays[i][0].y = (lv_value_precise_t)lroundf(c + DAY_RAY_R0 * si);
        s->rays[i][1].x = (lv_value_precise_t)lroundf(c + DAY_RAY_R1 * co);
        s->rays[i][1].y = (lv_value_precise_t)lroundf(c + DAY_RAY_R1 * si);
        lv_line_set_points(lv_obj_get_child(rays, (int32_t)i), s->rays[i], 2);
    }
}

static void set_y(void *obj, int32_t v) { lv_obj_set_y(obj, v); }

void scene_day_create(day_scene_t *s, lv_obj_t *parent)
{
    s->root = layer(parent);
    lv_obj_t *sun = ui_plain(s->root);
    lv_obj_set_size(sun, DAY_SUN_BOX, DAY_SUN_BOX);
    lv_obj_set_pos(sun, DAY_SUN_X, DAY_SUN_Y + DAY_RISE_PX);
    lv_obj_t *rays = ui_plain(sun);
    lv_obj_set_size(rays, DAY_SUN_BOX, DAY_SUN_BOX);
    for (int i = 0; i < DAY_RAYS; i++) {
        lv_obj_t *l = lv_line_create(rays);
        lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(l, DAY_SUN_BOX, DAY_SUN_BOX); /* fixed: the points move inside it */
        lv_obj_set_style_line_width(l, DAY_RAY_W, 0);
        lv_obj_set_style_line_color(l, lv_color_hex(COLOR_SUN), 0);
        lv_obj_set_style_line_rounded(l, true, 0);
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, rays);
    lv_anim_set_user_data(&a, s);
    lv_anim_set_custom_exec_cb(&a, rays_step); /* applied at once, so the rays have points before drawing */
    lv_anim_set_values(&a, 0, 3600 / DAY_RAYS);
    lv_anim_set_duration(&a, DAY_RAY_TURN_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_obj_t *disc = ui_circle(sun, DAY_SUN_BOX / 2, DAY_SUN_BOX / 2, DAY_SUN_D, COLOR_SUN);
    lv_obj_set_style_border_width(disc, DAY_SUN_BORDER, 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(COLOR_SUN_EDGE), 0);

    lv_anim_t r; /* the design's .sun-rise: up 110 px over 4 s, ease-in-out */
    lv_anim_init(&r);
    lv_anim_set_var(&r, sun);
    lv_anim_set_exec_cb(&r, set_y);
    lv_anim_set_values(&r, DAY_SUN_Y + DAY_RISE_PX, DAY_SUN_Y);
    lv_anim_set_duration(&r, DAY_RISE_MS);
    lv_anim_set_path_cb(&r, lv_anim_path_ease_in_out);
    lv_anim_start(&r);

    int16_t hx, hy;
    assets_ui_pos("ui/hills", &hx, &hy);
    ui_image(s->root, "ui/hills", hx, hy);
    s->cat = cat_create(s->root, CAT_SCALE_M);
    cat_set_origin(s->cat, DAY_CAT_X, DAY_CAT_Y);
    cat_set_state(s->cat, CAT_PROUD);
}
