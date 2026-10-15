idf_component_register(
    SRCS
        "main.c"
        "dash_model.c"
        "core/leaf_can.c"
        "core/dash_calc.c"
        "ui/ui_gauge.c"
        "ui/dash_ui.c"
        "ui/fonts/font_speed_120.c"
    INCLUDE_DIRS "." "core" "ui"
    REQUIRES esp_driver_twai esp_timer nvs_flash)

# core/ is portable C; this tells dash_calc.h it may read the menuconfig values.
target_compile_definitions(${COMPONENT_LIB} PRIVATE DASH_HAVE_SDKCONFIG)

# An sdkconfig written before the chip revision option existed can have ESP-IDF's
# "revisions <3.0" setting saved as a fixed value, which a v3.x selection can't
# clear. Stop here instead of building a bootloader the board will refuse.
if(CONFIG_DASH_CHIP_REV_V3 AND CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
    message(FATAL_ERROR
        "928 EV Dash is set to a v3.x chip, but sdkconfig still selects ESP32-P4 "
        "revisions <3.0. Delete sdkconfig and the build directory, then choose "
        "menuconfig -> 928 EV Dash -> ESP32-P4 chip revision -> v3.x again. (Or turn off "
        "Component config -> Hardware Settings -> Chip revision -> "
        "'Select ESP32-P4 revisions <3.0'.)")
endif()

# Newer LVGL (9.6+) deprecates lv_obj_add_flag()/lv_obj_remove_flag() in favour of
# per-flag setters that older 9.x releases don't have; keep the portable calls.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)
