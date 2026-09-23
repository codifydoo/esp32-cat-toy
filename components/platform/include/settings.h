#pragma once
#include <stdint.h>
#include "esp_err.h"

#define SETTINGS_VOLUME_DEFAULT 100 /* 0 dB; the codec curve is -50 dB at 0 %, 0 dB at 100 % */
#define SETTINGS_VOLUME_MAX     100 /* hard ceiling; the parent area cannot exceed it */

esp_err_t settings_init(void);
uint8_t settings_volume_cap(void);
void settings_set_volume_cap(uint8_t pct);
uint8_t settings_cat_variant(void); /* 0 orange, 1 grey, 2 tux */
void settings_set_cat_variant(uint8_t variant);
uint32_t settings_hidden_apps(void); /* bit i hides APPS[i] */
void settings_set_hidden_apps(uint32_t mask);
