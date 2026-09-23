#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#define ASSETS_BASE_PATH "/assets"

/* Mounts the read-only "assets" LittleFS partition at /assets. */
esp_err_t assets_init(void);

/* Loads /assets/img/<name>.bin (LVGL v9 RGB565A8) into PSRAM once and caches it.
 * Never returns NULL: a missing or invalid file logs one error and yields a 32 px magenta dot.
 * Call from the LVGL task only. */
const lv_image_dsc_t *assets_image(const char *name);

/* Top-left position the exporter recorded for a cropped UI image; (0, 0) if unknown. */
void assets_ui_pos(const char *name, int16_t *x, int16_t *y);
