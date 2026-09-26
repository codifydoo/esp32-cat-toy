#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* The cat picker (design "Pick your cat"; spec §3.13), and the first run that shows it after the boot animation.
 * LVGL task only. */

/* Builds the three pads into parent (a full-screen object). The current cat's pad has the thick ring. A tap
 * chooses at once (the thick ring, the proud face, a meow; the choice and "first run done" are saved, and the live
 * cats switch), and done runs PICK_CELEBRATE_MS later. Once the picker is deleted, the other variants' images are
 * released. */
void first_run_picker(lv_obj_t *parent, void (*done)(void));
/* "first run done" is missing (never in the soak build, which opens apps from boot on). */
bool first_run_due(void);
/* Shows the picker on its own screen, fading in over the boot animation. When it ends, the lock shows if it is
 * night, else the menu. */
void first_run_show(void);
bool first_run_showing(void);
/* Home on the first-run picker: the menu (or the lock) now. It saves nothing unless a cat was already chosen. */
void first_run_skip(void);
