#pragma once
#include <stdint.h>
#include "lvgl.h"

/* The parent entry (spec §3.1): two fingers held still for 5 s, on the menu or on the bedtime lock. */
typedef struct {
    void (*holding)(void); /* every 50 ms once the hold owns the touch (1 s): keep the screen lit */
    void (*done)(void);    /* once, at 5 s: open the parent area */
} hold_ring_cb_t;

/* While `screen` is the active screen and no app is open, a 50 ms timer feeds two-finger touches to the
 * hold tracker (gestures.h). Once the hold owns the touch, the rest of that touch is swallowed, so letting
 * go taps nothing, and a progress ring centered on (cx, cy) shows. The ring is a child of `screen`; the
 * timer goes when the ring is deleted with it. */
void hold_ring_attach(lv_obj_t *screen, int32_t cx, int32_t cy, const hold_ring_cb_t *cb);
