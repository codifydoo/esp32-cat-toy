cmake_minimum_required(VERSION 3.16)
# Wi-Fi credentials (spec §8) come from the git-ignored sdkconfig.secrets when it exists. Builds that pass
# their own SDKCONFIG_DEFAULTS (the diag, soak, perf and gallery variants) list it themselves if they need it.
if(NOT DEFINED SDKCONFIG_DEFAULTS AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.secrets")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.secrets")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cat_toy)
