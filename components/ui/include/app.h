#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cat.h"
#include "lvgl.h"
#include "sounds.h"

typedef struct {
    const char *id;
    const char *icon;       /* 240 px menu icon asset, e.g. "apps/voices" */
    const char *icon_small; /* 96 px icon for the parent area */
    uint32_t color;         /* active page dot and accents */
    sound_id_t name_sound;  /* spoken name */
    sound_id_t swipe_sound; /* whoosh + spoken name */
    cat_state_t react;      /* the menu cat's expression while this app is shown */
    void (*create)(lv_obj_t *screen); /* build into an empty black screen (LVGL task) */
    /* Stop timers and audio, and drop any pointers to the screen's objects (e.g. set them to
     * NULL); the screen itself and its LVGL event callbacks are deleted separately, slightly
     * later (once the menu has faded back in), and pointer input to the screen is cut before
     * destroy() runs but a press already mid-dispatch, or a timer already queued, can still
     * invoke an event callback in the gap. Every callback the app registered on the screen or
     * its children (tap handlers, timers, ...) must therefore check the app's own state first
     * and no-op if destroy() already ran, rather than assume it cannot be called afterward. */
    void (*destroy)(void);
    void (*on_shake)(void);           /* optional, NULL if unused */
    void (*on_tilt)(float x_deg, float y_deg); /* optional, NULL if unused */
} app_t;

extern const app_t *const APPS[];
extern const size_t APP_COUNT;
