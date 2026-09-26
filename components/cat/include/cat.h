#pragma once
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

typedef enum { CAT_SCALE_S, CAT_SCALE_M, CAT_SCALE_L, CAT_SCALE_COUNT } cat_scale_t;
typedef enum { CAT_VARIANT_ORANGE, CAT_VARIANT_GREY, CAT_VARIANT_TUX, CAT_VARIANT_COUNT } cat_variant_t;
typedef enum {
    CAT_HAPPY,
    CAT_LAUGHING,
    CAT_SURPRISED,
    CAT_LISTENING,
    CAT_TALKING,
    CAT_SLEEPY,
    CAT_SLEEPING,
    CAT_DIZZY,
    CAT_EATING,
    CAT_PROUD,
    CAT_YAWN,
    CAT_PURRING,
    CAT_SINGING,
    CAT_SHH,
    CAT_SWEEP,
    CAT_STATE_COUNT
} cat_state_t;
typedef enum {
    CAT_COSTUME_NONE,
    CAT_COSTUME_MOUSE,
    CAT_COSTUME_LION,
    CAT_COSTUME_ROBOT,
    CAT_COSTUME_MINER,
    CAT_COSTUME_COUNT
} cat_costume_t;

/* The chosen variant (spec §3.13); every live cat without a palette switches to it at once. */
void cat_set_variant(cat_variant_t variant);
/* "orange", "grey" or "tux": the variant's asset folder, also a valid cat_create_colored() palette. */
const char *cat_variant_name(cat_variant_t v);
lv_obj_t *cat_create(lv_obj_t *parent, cat_scale_t scale);
/* A cat in another exported palette instead of the chosen variant: a variant name (cat_variant_name(), the
 * cat picker), or "piano_c", "piano_d", "piano_e", "piano_g" or "piano_a" (Meow piano, spec §3.11, which
 * exist at CAT_SCALE_S only). palette must outlive the cat (use a string literal or cat_variant_name()). Such a
 * cat keeps its palette when the chosen variant changes. */
lv_obj_t *cat_create_colored(lv_obj_t *parent, cat_scale_t scale, const char *palette);
void cat_set_origin(lv_obj_t *cat, int32_t x, int32_t y);
void cat_set_state(lv_obj_t *cat, cat_state_t state);
void cat_set_costume(lv_obj_t *cat, cat_costume_t costume);
void cat_set_look(lv_obj_t *cat, float x, float y);
void cat_set_mouth(lv_obj_t *cat, uint8_t frame);
void cat_part_asset(char *out, size_t n, cat_scale_t scale, const char *part);
const char *cat_state_name(cat_state_t state);
