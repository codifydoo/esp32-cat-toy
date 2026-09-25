#pragma once
#include "app.h"

/* The parent area (spec 3.1, 3.12). Opened by a two-finger hold on the menu or on the bedtime lock, never
 * from the carousel, so it is not in APPS. Home tiles (Volume, Sleep), Max volume and Sleep times; M7 adds
 * Apps, Cat and the battery. */
extern const app_t PARENT_APP;
