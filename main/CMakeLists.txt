idf_component_register(
    SRCS
        "main.c"
        "dash_model.c"
        "core/leaf_can.c"
        "core/dash_calc.c"
        "ui/ui_gauge.c"
        "ui/dash_ui.c"
    INCLUDE_DIRS "." "core" "ui"
    REQUIRES esp_driver_twai esp_timer nvs_flash)

# Newer LVGL (9.6+) deprecates lv_obj_add_flag()/lv_obj_remove_flag() in favour of
# per-flag setters that older 9.x releases don't have; keep the portable calls.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)
