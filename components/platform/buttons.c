#include "buttons.h"
#include <stdbool.h>
#include "bsp/esp-bsp.h"
#include "debounce.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#define HOME_GPIO GPIO_NUM_0 /* BOOT button, low while pressed */

/* On the 1.75C, PWR is the AXP2101 PWRON key: the PMU debounces it and latches a
 * "short press" flag (released before the long-press time). A long hold (about 7 s) powers the board
 * off in hardware. */
#define PMU_ADDR          0x34
#define PMU_REG_INTEN2    0x41
#define PMU_REG_INTSTS2   0x49
#define PMU_PKEY_SHORT    (1u << 3) /* AXP2101 IRQ bit 11 */
#define PMU_POLL_EVERY    3         /* read the PMU every 3rd 20 ms poll (60 ms) */
#define PMU_TIMEOUT_MS    50

static const char *TAG = "buttons";
static i2c_master_dev_handle_t s_pmu;
static debounce_t s_home;
static uint8_t s_pmu_tick;

static bool home_raw(void) { return gpio_get_level(HOME_GPIO) == 0; }

static esp_err_t pmu_read(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_pmu, &reg, 1, val, 1, PMU_TIMEOUT_MS);
}

static esp_err_t pmu_write(uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_pmu, buf, sizeof(buf), PMU_TIMEOUT_MS);
}

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

static void pmu_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (i2c_master_probe(bus, PMU_ADDR, PMU_TIMEOUT_MS) != ESP_OK) {
        ESP_LOGW(TAG, "PMU missing; PWR button disabled");
        return;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PMU_ADDR,
        .scl_speed_hz = 100000,
    };
    uint8_t en = 0;
    if (i2c_master_bus_add_device(bus, &cfg, &s_pmu) != ESP_OK || pmu_read(PMU_REG_INTEN2, &en) != ESP_OK ||
        pmu_write(PMU_REG_INTEN2, en | PMU_PKEY_SHORT) != ESP_OK) {
        ESP_LOGW(TAG, "PMU not responding; PWR button disabled");
        if (s_pmu) {
            i2c_master_bus_rm_device(s_pmu);
            s_pmu = NULL;
        }
        return;
    }
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
    pmu_init();
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
