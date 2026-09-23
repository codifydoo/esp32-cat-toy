#include "buttons.h"
#include <stdbool.h>
#include "bsp/esp-bsp.h"
#include "debounce.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_io_expander.h"

#define HOME_GPIO GPIO_NUM_0            /* BOOT button, low while pressed */
#define POWER_PIN IO_EXPANDER_PIN_NUM_4 /* TCA9554 EXIO4 = SYS_OUT, high while PWR is pressed */

static const char *TAG = "buttons";
static esp_io_expander_handle_t s_expander;
static debounce_t s_home, s_power;

static bool home_raw(void) { return gpio_get_level(HOME_GPIO) == 0; }

static bool power_raw(void)
{
    uint32_t levels = 0;
    if (!s_expander || esp_io_expander_get_level(s_expander, POWER_PIN, &levels) != ESP_OK) {
        return false;
    }
    return (levels & POWER_PIN) != 0;
}

esp_err_t buttons_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << HOME_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "BOOT gpio");
    s_expander = bsp_io_expander_init();
    if (s_expander) {
        esp_io_expander_set_dir(s_expander, POWER_PIN, IO_EXPANDER_INPUT);
    } else {
        ESP_LOGW(TAG, "IO expander missing; PWR button disabled");
    }
    debounce_init(&s_home, home_raw());
    debounce_init(&s_power, power_raw());
    return ESP_OK;
}

uint8_t buttons_poll(void)
{
    uint8_t ev = 0;
    if (debounce_update(&s_home, home_raw())) {
        ev |= BUTTON_EV_HOME;
    }
    if (debounce_update(&s_power, power_raw())) {
        ev |= BUTTON_EV_POWER;
    }
    return ev;
}
