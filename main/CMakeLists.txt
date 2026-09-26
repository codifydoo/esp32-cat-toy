idf_component_register(SRC_DIRS "." INCLUDE_DIRS "."
                       REQUIRES platform assets cat ui
                       PRIV_REQUIRES waveshare__esp32_s3_touch_amoled_1_75c esp_timer esp_driver_gpio)

set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
if(NOT EXISTS "${ASSETS_DIR}")
    message(FATAL_ERROR "assets/ is missing. Run tools/build_assets.sh first.")
endif()
littlefs_create_partition_image(assets ${ASSETS_DIR} FLASH_IN_PROJECT)
