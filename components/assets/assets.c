#include "assets.h"
#include <stdio.h>
#include <string.h>
#include "assets_manifest.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"

#define PLACEHOLDER_PX 32

typedef struct entry {
    struct entry *next;
    char name[48];
    lv_image_dsc_t dsc;
    uint8_t *buf; /* NULL when dsc is a copy of the placeholder */
} entry_t;

static const char *TAG = "assets";
static entry_t *s_cache;
static lv_image_dsc_t s_placeholder;
static uint16_t s_placeholder_buf[PLACEHOLDER_PX * PLACEHOLDER_PX * 3 / 2]; /* RGB565 plane + A8 plane */

static void build_placeholder(void)
{
    uint8_t *alpha = (uint8_t *)s_placeholder_buf + PLACEHOLDER_PX * PLACEHOLDER_PX * 2;
    const int r = PLACEHOLDER_PX / 2;
    for (int y = 0; y < PLACEHOLDER_PX; y++) {
        for (int x = 0; x < PLACEHOLDER_PX; x++) {
            int dx = x - r, dy = y - r, i = y * PLACEHOLDER_PX + x;
            s_placeholder_buf[i] = 0xF81F; /* magenta */
            alpha[i] = dx * dx + dy * dy <= r * r ? 255 : 0;
        }
    }
    s_placeholder.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_placeholder.header.cf = LV_COLOR_FORMAT_RGB565A8;
    s_placeholder.header.w = PLACEHOLDER_PX;
    s_placeholder.header.h = PLACEHOLDER_PX;
    s_placeholder.header.stride = PLACEHOLDER_PX * 2;
    s_placeholder.data = (const uint8_t *)s_placeholder_buf;
    s_placeholder.data_size = sizeof(s_placeholder_buf);
}

esp_err_t assets_init(void)
{
    build_placeholder();
    esp_vfs_littlefs_conf_t conf = {
        .base_path = ASSETS_BASE_PATH,
        .partition_label = "assets",
        .read_only = true,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
    }
    return err;
}

/* Reads and validates one image file; returns the buffer (header + planes) or NULL. */
static uint8_t *read_image(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "missing image %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = size > (long)sizeof(lv_image_header_t) ? heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM) : NULL;
    bool ok = buf && fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    if (ok) {
        lv_image_header_t hdr;
        memcpy(&hdr, buf, sizeof(hdr));
        size_t need = sizeof(hdr) + (size_t)hdr.stride * hdr.h + (size_t)hdr.w * hdr.h;
        ok = hdr.magic == LV_IMAGE_HEADER_MAGIC && hdr.cf == LV_COLOR_FORMAT_RGB565A8 && (size_t)size >= need;
    }
    if (!ok) {
        ESP_LOGE(TAG, "bad image %s", path);
        heap_caps_free(buf);
        return NULL;
    }
    *size_out = (size_t)size;
    return buf;
}

const lv_image_dsc_t *assets_image(const char *name)
{
    for (entry_t *e = s_cache; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return &e->dsc;
        }
    }
    entry_t *e = heap_caps_calloc(1, sizeof(entry_t), MALLOC_CAP_SPIRAM);
    if (!e) {
        return &s_placeholder;
    }
    strlcpy(e->name, name, sizeof(e->name));
    char path[96];
    snprintf(path, sizeof(path), ASSETS_BASE_PATH "/img/%s.bin", name);
    size_t size = 0;
    e->buf = read_image(path, &size);
    if (e->buf) {
        memcpy(&e->dsc.header, e->buf, sizeof(lv_image_header_t));
        e->dsc.data = e->buf + sizeof(lv_image_header_t);
        e->dsc.data_size = (uint32_t)(size - sizeof(lv_image_header_t));
    } else {
        e->dsc = s_placeholder; /* cached so the error is logged once, not every frame */
    }
    e->next = s_cache;
    s_cache = e;
    return &e->dsc;
}

void assets_ui_pos(const char *name, int16_t *x, int16_t *y)
{
    *x = 0;
    *y = 0;
    for (size_t i = 0; i < UI_IMAGE_POS_COUNT; i++) {
        if (strcmp(UI_IMAGE_POS[i].name, name) == 0) {
            *x = UI_IMAGE_POS[i].x;
            *y = UI_IMAGE_POS[i].y;
            return;
        }
    }
}
