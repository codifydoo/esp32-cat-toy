#pragma once
#include "app.h"

/* The parent area (spec §3.1, §3.12, §3.13). Opened by a two-finger hold on the menu or on the bedtime lock,
 * never from the carousel, so it is not in APPS. Home tiles (Volume, Sleep, Apps, Cat) and the battery; Max
 * volume, Sleep times, Show apps and the cat picker. */
extern const app_t PARENT_APP;
