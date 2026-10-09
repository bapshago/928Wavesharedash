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
  Two units, one firmware: `dash_ui_create(screen, side)` builds the LEFT (speed/RND/odo/SOC)
  or RIGHT (current/voltage/12V/temps/status) driver page; side comes from Kconfig
  `DASH_SIDE_LEFT/RIGHT`. Gauge text/ticks scale with radius (`ui_gauge.c`).
- `main/ui/fonts/font_speed_120.c` — generated digits-only font (command in its header).
- `main/dash_model.c` — ESP32 implementation of `dash_model.h` (TWAI, NVS, tasks).
- `sim/sim_model.c` — desktop implementation of `dash_model.h`; `sim/` renders PNGs.
- `hardware/mechanical/` — DXF cut files, bend diagrams (SVG), build photos and the mechanical
  README. File names and part names are referenced from that README and the main README:
  rename them together, and also in `docs/BUILD.md` (the end-to-end build guide). Resize photos to ~1400 px (strips phone metadata) before committing.

## Checks to run after changes

- `cmake -S host_test -B build-test && cmake --build build-test && ./build-test/test_core`
- `cmake -S sim -B build-sim && cmake --build build-sim -j && ./build-sim/dash_sim --all /tmp/shots`
  and look at the PNGs (both `_left` and `_right`) — layout must stay inside the 800 px circle.
- Firmware: `idf.py build` (ESP-IDF ≥ 5.5, target esp32p4; verified on 5.5 and 6.1).

## Gotchas

- UI calls from outside LVGL timers must hold `bsp_display_lock()`.
- Prefix local helpers (`prefs_*` in dash_model.c): ESP-IDF 6.1 added `nvs_get_double()` etc., and
  unprefixed names like `nvs_*` collide with new IDF APIs.
- Kconfig `bool` options are undefined (not 0) when off — use `#if CONFIG_X`, not the macro as a value.
- BSP pinned to `waveshare/esp32_p4_wifi6_touch_lcd_xc` 3.0.1 (same as Waveshare's examples);
  its display config uses `esp_lvgl_adapter` (`ESP_LV_ADAPTER_DEFAULT_CONFIG()`).
- LVGL is pinned to ~9.5 (main/idf_component.yml, sim/CMakeLists.txt, sim/lv_conf.h): 9.6 moved its
  headers and esp_lvgl_adapter's old includes then fail under ESP-IDF 6.x -Werror. Keep all three in step.
- Screenshots in `docs/screenshots/` come from the simulator; recompress before committing
  (the simulator's PNG writer stores data uncompressed).
