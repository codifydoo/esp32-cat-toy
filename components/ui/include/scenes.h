#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "tokens.h"

/* The Sleepy clock's scenes (Screen.dc.html clock view; spec §3.12). Each builds into `parent` and runs
 * its own animations. Every animation targets one of the scene's objects, so deleting the parent (or
 * cleaning it) stops them all. */

typedef struct {
    lv_obj_t *root; /* the whole scene: hide it to stop drawing, move it for the burn-in shift */
    lv_obj_t *cat;  /* the sleeping cat, scale M */
    lv_obj_t *hour[HOUR_STARS];
    uint8_t lit;    /* hour stars lit now */
} night_scene_t;

/* The moon, nine twinkling stars, the sleeping cat and five unlit hour stars. */
void scene_night_create(night_scene_t *s, lv_obj_t *parent);
/* Lights the first `lit` hour stars from the left (at most HOUR_STARS). */
void scene_night_set_stars(night_scene_t *s, uint8_t lit);

typedef struct {
    lv_obj_t *root;
    lv_obj_t *cat; /* the proud cat, scale M */
    lv_point_precise_t rays[DAY_RAYS][2]; /* the turning rays' end points: lv_line keeps a pointer */
} day_scene_t;

/* The sun with its turning rays, rising DAY_RISE_PX over DAY_RISE_MS; the hills; the proud cat. `s`
 * must outlive the scene's objects, because the rays read their points from it. */
void scene_day_create(day_scene_t *s, lv_obj_t *parent);
