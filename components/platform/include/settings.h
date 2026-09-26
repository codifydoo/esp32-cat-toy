#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SETTINGS_VOLUME_DEFAULT 100 /* 0 dB; the codec curve is -50 dB at 0 %, 0 dB at 100 % */
#define SETTINGS_VOLUME_MAX     100 /* hard ceiling; the parent area cannot exceed it */
#define SETTINGS_BEDTIME_DEFAULT 1170 /* 19:30, minutes of the day (spec §4.5) */
#define SETTINGS_WAKE_DEFAULT    420  /* 07:00 */

esp_err_t settings_init(void);
uint8_t settings_volume_cap(void);
void settings_set_volume_cap(uint8_t pct);
uint8_t settings_cat_variant(void); /* 0 orange, 1 grey, 2 tux */
void settings_set_cat_variant(uint8_t variant);
uint32_t settings_hidden_apps(void); /* bit i hides APPS[i] */
void settings_set_hidden_apps(uint32_t mask);
uint16_t settings_bedtime(void); /* minute of the day, 0-1439 */
void settings_set_bedtime(uint16_t min);
uint16_t settings_wake(void);
void settings_set_wake(uint16_t min);
bool settings_first_run_done(void); /* the cat picker ran once (spec §3.13); false while the key is missing */
void settings_set_first_run_done(void);
