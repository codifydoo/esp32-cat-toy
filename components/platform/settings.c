#include "settings.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings";
static nvs_handle_t s_nvs;
static uint8_t s_volume = SETTINGS_VOLUME_DEFAULT;
static uint8_t s_variant;
static uint32_t s_hidden;

static uint8_t clamp_volume(uint8_t v) { return v > SETTINGS_VOLUME_MAX ? SETTINGS_VOLUME_MAX : v; }

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
    s_volume = clamp_volume(s_volume);
    if (s_variant > 2) {
        s_variant = 0;
    }
    ESP_LOGI(TAG, "volume cap %u%%, cat %u, hidden apps 0x%08lx", s_volume, s_variant, (unsigned long)s_hidden);
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
