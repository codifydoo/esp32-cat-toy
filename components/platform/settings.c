#include "settings.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "schedule.h"

static const char *TAG = "settings";
static nvs_handle_t s_nvs;
static uint8_t s_volume = SETTINGS_VOLUME_DEFAULT;
static uint8_t s_variant;
static uint32_t s_hidden;
static uint16_t s_bed = SETTINGS_BEDTIME_DEFAULT, s_wake = SETTINGS_WAKE_DEFAULT;

static uint8_t clamp_volume(uint8_t v) { return v > SETTINGS_VOLUME_MAX ? SETTINGS_VOLUME_MAX : v; }
static uint16_t valid_min(uint16_t v, uint16_t fallback) { return v < SCHEDULE_DAY_MIN ? v : fallback; }

esp_err_t settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS unusable (%s); erasing and using defaults", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "init after erase");
    }
    ESP_RETURN_ON_ERROR(nvs_open("cat", NVS_READWRITE, &s_nvs), TAG, "open");
    nvs_get_u8(s_nvs, "vol", &s_volume); /* NOT_FOUND leaves the default in place */
    nvs_get_u8(s_nvs, "variant", &s_variant);
    nvs_get_u32(s_nvs, "hidden", &s_hidden);
    nvs_get_u16(s_nvs, "bed", &s_bed);
    nvs_get_u16(s_nvs, "wake", &s_wake);
    s_bed = valid_min(s_bed, SETTINGS_BEDTIME_DEFAULT);
    s_wake = valid_min(s_wake, SETTINGS_WAKE_DEFAULT);
    s_volume = clamp_volume(s_volume);
    if (s_variant > 2) {
        s_variant = 0;
    }
    ESP_LOGI(TAG, "volume cap %u%%, cat %u, hidden apps 0x%08lx", s_volume, s_variant, (unsigned long)s_hidden);
    ESP_LOGI(TAG, "bedtime %02u:%02u, wake-up %02u:%02u", s_bed / 60, s_bed % 60, s_wake / 60, s_wake % 60);
    return ESP_OK;
}

static void commit(esp_err_t err, const char *key)
{
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving %s failed: %s", key, esp_err_to_name(err));
    }
}

uint8_t settings_volume_cap(void) { return s_volume; }
void settings_set_volume_cap(uint8_t pct)
{
    s_volume = clamp_volume(pct);
    commit(nvs_set_u8(s_nvs, "vol", s_volume), "vol");
}

uint8_t settings_cat_variant(void) { return s_variant; }
void settings_set_cat_variant(uint8_t variant)
{
    s_variant = variant > 2 ? 0 : variant;
    commit(nvs_set_u8(s_nvs, "variant", s_variant), "variant");
}

uint32_t settings_hidden_apps(void) { return s_hidden; }
void settings_set_hidden_apps(uint32_t mask)
{
    s_hidden = mask;
    commit(nvs_set_u32(s_nvs, "hidden", s_hidden), "hidden");
}

uint16_t settings_bedtime(void) { return s_bed; }
void settings_set_bedtime(uint16_t min)
{
    s_bed = valid_min(min, SETTINGS_BEDTIME_DEFAULT);
    commit(nvs_set_u16(s_nvs, "bed", s_bed), "bed");
}

uint16_t settings_wake(void) { return s_wake; }
void settings_set_wake(uint16_t min)
{
    s_wake = valid_min(min, SETTINGS_WAKE_DEFAULT);
    commit(nvs_set_u16(s_nvs, "wake", s_wake), "wake");
}
