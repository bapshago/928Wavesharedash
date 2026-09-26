// Round needle gauge: the LVGL version of createGauge()/updateGauge() in the
// web dash's driverpage.h. 270° sweep starting at the bottom-left, blue value
// arc, white needle, optional red/green zones, value readout under the pivot.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_GAUGE_MAX_ZONES  3
#define UI_GAUGE_MAX_LABELS 13

typedef struct {
    float    start;  // fraction of the sweep, 0..1
    float    end;
    uint32_t color;  // 0xRRGGBB
} ui_zone_t;

typedef struct {
    const char *name;        // "Voltage", "Speed", ...
    const char *unit;        // shown after the value ("V") or under it on large gauges
    float       min;         // min may be greater than max for an inverted scale
    float       max;
    int32_t     radius;      // px
    bool        large;       // numbered ticks, big readout, unit on its own line
    uint8_t     major_ticks; // number of numbered intervals on large gauges (e.g. 8 → 0,20..160)
    uint8_t     decimals;    // digits after the point in the readout (web dash: 0)
    ui_zone_t   zones[UI_GAUGE_MAX_ZONES];
    uint8_t     zone_count;
} ui_gauge_cfg_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *scale;
    lv_obj_t *arc;
    lv_obj_t *needle;
    lv_obj_t *value_lbl;
    lv_obj_t *unit_lbl;
    lv_point_precise_t pts[2];
    const char *labels[UI_GAUGE_MAX_LABELS + 1];
    char        label_buf[UI_GAUGE_MAX_LABELS][8];
    char        unit[8];
    float       min;
    float       max;
    int32_t     radius;
    bool        large;
    uint8_t     major_ticks;
    uint8_t     decimals;
    float       shown_pct;   // smoothed needle position (the web used a 0.3 s CSS transition)
    float       drawn_pct;   // position last drawn, to skip redundant redraws
    bool        arc_red;
} ui_gauge_t;

// Create the gauge centred at (cx, cy) inside parent.
void ui_gauge_create(ui_gauge_t *g, lv_obj_t *parent, const ui_gauge_cfg_t *cfg, int32_t cx, int32_t cy);

// Change the scale (e.g. kph ↔ mph, °C ↔ °F) and relabel. major_ticks = 0 keeps the current count.
void ui_gauge_set_range(ui_gauge_t *g, float min, float max, const char *unit, uint8_t major_ticks);

// Update the reading. NAN shows "--" and parks the needle at the start.
// Call periodically: the needle eases toward the value on each call.
void ui_gauge_set_value(ui_gauge_t *g, float value);

#ifdef __cplusplus
}
#endif
