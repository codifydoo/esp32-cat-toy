cmake_minimum_required(VERSION 3.16)
project(cat_toy_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(unity GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git GIT_TAG v2.6.0)
FetchContent_MakeAvailable(unity)

# Our code is strict; Unity (added above) is not affected by these flags.
add_compile_options(-Wall -Wextra -Werror)

set(LOGIC_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/logic)
file(GLOB LOGIC_SRCS CONFIGURE_DEPENDS ${LOGIC_DIR}/*.c)
add_library(logic STATIC ${LOGIC_SRCS})
target_include_directories(logic PUBLIC ${LOGIC_DIR}/include)
target_link_libraries(logic PUBLIC m)

enable_testing()
file(GLOB TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/test_*.c)
foreach(src ${TEST_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE logic unity)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/fxcli.c)
    add_executable(fxcli ${CMAKE_CURRENT_LIST_DIR}/fxcli.c)
    target_link_libraries(fxcli PRIVATE logic)
endif()
