#include "display.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "touch_raw.h"

static const char *TAG = "display";
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static uint8_t s_brightness = 100;

/* The CO5300 needs even start and odd end coordinates (same rounding as the BSP). */
static void rounder_cb(lv_event_t *e)
{
    lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
    a->x1 = (a->x1 >> 1) << 1;
    a->y1 = (a->y1 >> 1) << 1;
    a->x2 = ((a->x2 >> 1) << 1) + 1;
    a->y2 = ((a->y2 >> 1) << 1) + 1;
}

/* bsp_display_start_with_config() with one change: touch goes through touch_raw_read() so the
 * parent gesture can see two fingers. */
esp_err_t display_init(void)
{
    esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter.task_core_id = 1; /* audio and sensors run on core 0 */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter), TAG, "adapter init");

    const bsp_display_config_t panel_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8,
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&panel_cfg, &s_panel, &s_io), TAG, "panel");
    const esp_lv_adapter_display_config_t disp_cfg = {
        .panel = s_panel,
        .panel_io = s_io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = true,
            .enable_ppa_accel = false,
            .require_double_buffer = true,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
    };
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "register display");
    lv_display_add_event_cb(disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    const bsp_display_cfg_t bsp_touch_cfg = { /* bsp_touch_new() reads only touch_flags */
        .touch_flags = {.swap_xy = 0, .mirror_x = 1, .mirror_y = 1}, /* verified in the Task 13 touch check */
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(bsp_touch_new(&bsp_touch_cfg, &tp), TAG, "touch");
    esp_lv_adapter_touch_config_t touch = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    touch.callbacks.custom_touch_read = touch_raw_read;
    ESP_RETURN_ON_FALSE(esp_lv_adapter_register_touch(&touch), ESP_FAIL, TAG, "register touch");

    display_set_brightness(100);
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "adapter start");
    return ESP_OK;
}

void display_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    /* CO5300 "write display brightness" (0x51) in the QSPI command format, as the BSP sends it. */
    uint8_t level = (uint8_t)(pct * 255 / 100);
    if (s_io && esp_lcd_panel_io_tx_param(s_io, (0x02u << 24) | (0x51u << 8), &level, 1) == ESP_OK) {
        s_brightness = pct;
    }
}

uint8_t display_brightness(void) { return s_brightness; }
bool display_lock(int32_t timeout_ms) { return esp_lv_adapter_lock(timeout_ms) == ESP_OK; }
void display_unlock(void) { esp_lv_adapter_unlock(); }

void display_sleep(void)
{
    display_set_brightness(0);
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, false);
        esp_lcd_panel_disp_sleep(s_panel, true); /* CO5300 sleep-in; the restart after deep sleep wakes it */
    }
}
