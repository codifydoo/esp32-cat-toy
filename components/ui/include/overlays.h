#pragma once
#include <stdbool.h>

/* The low-battery and charging screens (design lowbatt and charging; spec §3.13): OVERLAY_MS over whatever shows,
 * on LVGL's top layer, taking every touch. LVGL task only. */
typedef enum {
    OVERLAY_HUNGRY,   /* the sleepy cat, the empty bowl, the blinking red battery, "Gladna sam…" */
    OVERLAY_CHARGING, /* the eating cat, the full bowl, the filling green battery, munching and "Njam njam!" */
} overlay_kind_t;

/* Shows it for OVERLAY_MS; done (may be NULL) runs when it ends, early or not. Nothing if one already shows. */
void overlay_show(overlay_kind_t kind, void (*done)(void));
bool overlay_showing(void);
/* Ends it now (Home, PWR); done still runs. */
void overlay_close(void);
