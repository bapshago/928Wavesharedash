# CLAUDE.md — 928 Waveshare Dash

Round-screen dash for a Porsche 928 EV (Nissan Leaf drivetrain + ZombieVerter VCU)
on the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C (800×800 round MIPI-DSI, touch).
Ported from the web dash in bapshago/AngryPixieDash (`leaf_dash_v9`): keep behaviour
aligned with it unless a change is deliberate and noted in README "Differences".

## Layout / boundaries

- `main/core/` — portable C only (no ESP-IDF, no LVGL): `dash_state.h`, `leaf_can.c`
  (CAN decoder), `dash_calc.c` (speed, range, ETA, odometer, test data), `dash_model.h`
  (interface the UI reads). Unit-tested in `host_test/`.
- `main/ui/` — LVGL 9 only, reads data via `dash_model_snapshot()`; never touches drivers.
- `main/dash_model.c` — ESP32 implementation of `dash_model.h` (TWAI, NVS, tasks).
- `sim/sim_model.c` — desktop implementation of `dash_model.h`; `sim/` renders PNGs.

## Checks to run after changes

- `cmake -S host_test -B build-test && cmake --build build-test && ./build-test/test_core`
- `cmake -S sim -B build-sim && cmake --build build-sim -j && ./build-sim/dash_sim --all /tmp/shots`
  and look at the PNGs — layout must stay inside the 800 px circle.
- Firmware: `idf.py build` (ESP-IDF ≥ 5.5, target esp32p4).

## Gotchas

- UI calls from outside LVGL timers must hold `bsp_display_lock()`.
- Kconfig `bool` options are undefined (not 0) when off — use `#if CONFIG_X`, not the macro as a value.
- BSP pinned to `waveshare/esp32_p4_wifi6_touch_lcd_xc` 3.0.1 (same as Waveshare's examples);
  its display config uses `esp_lvgl_adapter` (`ESP_LV_ADAPTER_DEFAULT_CONFIG()`).
- Stick to LVGL APIs present across 9.x (e.g. `lv_obj_add_flag`, deprecated in 9.6 but portable).
- Screenshots in `docs/screenshots/` come from the simulator; recompress before committing
  (the simulator's PNG writer stores data uncompressed).
