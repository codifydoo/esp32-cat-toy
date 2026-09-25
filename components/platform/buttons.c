#include "buttons.h"
#include <stdbool.h>
#include "debounce.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "pmu.h"

#define HOME_GPIO GPIO_NUM_0 /* BOOT button, low while pressed */

/* On the 1.75C, PWR is the AXP2101 PWRON key: the PMU debounces it and latches a
 * "short press" flag (released before the long-press time). A long hold (about 7 s) powers the board
 * off in hardware. */
#define PMU_REG_INTEN2    0x41
#define PMU_REG_INTSTS2   0x49
#define PMU_PKEY_SHORT    (1u << 3) /* AXP2101 IRQ bit 11 */
#define PMU_POLL_EVERY    3         /* read the PMU every 3rd 20 ms poll (60 ms) */

static const char *TAG = "buttons";
static bool s_pmu; /* the PWR key flag is enabled */
static debounce_t s_home;
static uint8_t s_pmu_tick;

static bool home_raw(void) { return gpio_get_level(HOME_GPIO) == 0; }

/* Returns true once per short PWR press, and clears the latched flag. */
static bool power_pressed(void)
{
    uint8_t sts = 0;
    if (!s_pmu || pmu_read(PMU_REG_INTSTS2, &sts) != ESP_OK || !(sts & PMU_PKEY_SHORT)) {
        return false;
    }
    /* Write-1-to-clear, leaves the other flags. If the clear fails, report the press on the poll
     * that does clear it, so one press never fires twice. */
    return pmu_write(PMU_REG_INTSTS2, PMU_PKEY_SHORT) == ESP_OK;
}

static void pmu_key_init(void)
{
    uint8_t en = 0;
    if (!pmu_init() || pmu_read(PMU_REG_INTEN2, &en) != ESP_OK ||
        pmu_write(PMU_REG_INTEN2, en | PMU_PKEY_SHORT) != ESP_OK) {
        ESP_LOGW(TAG, "PMU not responding; PWR button disabled");
        return;
    }
    s_pmu = true;
    /* Drop a press latched before boot (e.g. the click that powered the board on). */
    if (pmu_write(PMU_REG_INTSTS2, PMU_PKEY_SHORT) != ESP_OK) {
        ESP_LOGW(TAG, "PMU flag clear failed; a PWR press from before boot may count once");
    }
}

esp_err_t buttons_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << HOME_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "BOOT gpio");
    pmu_key_init();
    debounce_init(&s_home, home_raw());
    return ESP_OK;
}

uint8_t buttons_poll(void)
{
    uint8_t ev = 0;
    if (debounce_update(&s_home, home_raw())) {
        ev |= BUTTON_EV_HOME;
    }
    if (++s_pmu_tick >= PMU_POLL_EVERY) {
        s_pmu_tick = 0;
        if (power_pressed()) {
            ev |= BUTTON_EV_POWER;
        }
    }
    return ev;
}
