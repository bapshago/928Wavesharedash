# Desktop build of the dash UI: renders PNG screenshots without the board.
#
#   cmake -S sim -B build-sim && cmake --build build-sim -j
#   ./build-sim/dash_sim --all docs/screenshots
#
# LVGL is fetched from GitHub; pass -DLVGL_DIR=/path/to/lvgl to use a local copy.
cmake_minimum_required(VERSION 3.16)
project(dash_sim C)

set(CMAKE_C_STANDARD 11)
set(LVGL_VERSION "v9.5.0" CACHE STRING "LVGL tag to fetch")
set(LVGL_DIR "" CACHE PATH "Local LVGL checkout (skips the download)")

if(NOT LVGL_DIR)
    include(FetchContent)
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG ${LVGL_VERSION}
        GIT_SHALLOW TRUE)
    FetchContent_GetProperties(lvgl)
    if(NOT lvgl_POPULATED)
        FetchContent_Populate(lvgl)
    endif()
    set(LVGL_DIR ${lvgl_SOURCE_DIR})
endif()

file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -w)

set(MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main)
add_executable(dash_sim
    sim_main.c
    sim_model.c
    png_write.c
    ${MAIN_DIR}/core/leaf_can.c
    ${MAIN_DIR}/core/dash_calc.c
    ${MAIN_DIR}/ui/ui_gauge.c
    ${MAIN_DIR}/ui/dash_ui.c)
target_include_directories(dash_sim PRIVATE ${MAIN_DIR}/core ${MAIN_DIR}/ui)
target_compile_options(dash_sim PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(dash_sim PRIVATE lvgl m)
