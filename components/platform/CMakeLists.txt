idf_component_register(SRC_DIRS "." INCLUDE_DIRS "include"
                       REQUIRES assets espressif__esp_lcd_touch
                       PRIV_REQUIRES logic nvs_flash esp_driver_gpio esp_wifi esp_netif esp_event esp_timer)
