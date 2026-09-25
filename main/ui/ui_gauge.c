#include "ui_gauge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_theme.h"

#define SWEEP_DEG  270
#define START_DEG  135   // LVGL angles: 0° = 3 o'clock, clockwise; 135° = bottom-left
#define SCALE_MAX  1000  // the scale runs 0..1000 (per-mille of the sweep)
#define EASE       0.4f  // fraction of the remaining distance the needle moves per update

static int32_t arc_width(const ui_gauge_t *g) { return g->large ? 8 : 5; }
static int32_t zone_width(const ui_gauge_t *g) { return g->large ? 5 : 4; }
static int32_t cont_size(const ui_gauge_t *g) { return 2 * (g->radius + zone_width(g) + 3); }

static void relabel(ui_gauge_t *g)
{
    if (!g->large) {
        return;
    }
    lv_scale_set_total_tick_count(g->scale, g->major_ticks * 4 + 1);
    lv_scale_set_major_tick_every(g->scale, 4);
    for (int i = 0; i <= g->major_ticks && i < UI_GAUGE_MAX_LABELS; i++) {
        float v = g->min + (g->max - g->min) * (float)i / g->major_ticks;
        snprintf(g->label_buf[i], sizeof(g->label_buf[i]), "%ld", lroundf(v));
        g->labels[i] = g->label_buf[i];
    }
    g->labels[g->major_ticks + 1] = NULL;
    lv_scale_set_text_src(g->scale, g->labels);
}

static lv_obj_t *make_arc(lv_obj_t *parent, int32_t size, int32_t width)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    return arc;
}

void ui_gauge_create(ui_gauge_t *g, lv_obj_t *parent, const ui_gauge_cfg_t *cfg, int32_t cx, int32_t cy)
{
    memset(g, 0, sizeof(*g));
    g->min = cfg->min;
    g->max = cfg->max;
    g->radius = cfg->radius;
    g->large = cfg->large;
    g->decimals = cfg->decimals;
    g->major_ticks = cfg->major_ticks ? cfg->major_ticks : 8;
    if (g->major_ticks >= UI_GAUGE_MAX_LABELS) {
        g->major_ticks = UI_GAUGE_MAX_LABELS - 1;
    }
    snprintf(g->unit, sizeof(g->unit), "%s", cfg->unit ? cfg->unit : "");

    const int32_t size = cont_size(g);
    const int32_t c = size / 2;

    g->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(g->cont);
    lv_obj_set_size(g->cont, size, size);
    lv_obj_set_pos(g->cont, cx - c, cy - c);
    lv_obj_remove_flag(g->cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Red / green zones sit just outside the value arc.
    for (int i = 0; i < cfg->zone_count && i < UI_GAUGE_MAX_ZONES; i++) {
        const ui_zone_t *z = &cfg->zones[i];
        lv_obj_t *za = make_arc(g->cont, 2 * (g->radius + zone_width(g) + 2), zone_width(g));
        lv_arc_set_bg_angles(za, START_DEG + z->start * SWEEP_DEG, START_DEG + z->end * SWEEP_DEG);
        lv_obj_set_style_arc_color(za, lv_color_hex(z->color), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(za, LV_OPA_TRANSP, LV_PART_INDICATOR);
    }

    // Value arc over a dim track.
    g->arc = make_arc(g->cont, 2 * g->radius, arc_width(g));
    lv_arc_set_rotation(g->arc, START_DEG);
    lv_arc_set_bg_angles(g->arc, 0, SWEEP_DEG);
    lv_arc_set_range(g->arc, 0, SCALE_MAX);
    lv_arc_set_value(g->arc, 0);
    lv_obj_set_style_arc_color(g->arc, lv_color_hex(UI_COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g->arc, lv_color_hex(UI_COLOR_ARC), LV_PART_INDICATOR);

    // Tick marks (and numbers on large gauges) just inside the arc.
    g->scale = lv_scale_create(g->cont);
    const int32_t scale_d = 2 * (g->radius - arc_width(g) - 2);
    lv_obj_set_size(g->scale, scale_d, scale_d);
    lv_obj_center(g->scale);
    lv_scale_set_mode(g->scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(g->scale, 0, SCALE_MAX);
    lv_scale_set_angle_range(g->scale, SWEEP_DEG);
    lv_scale_set_rotation(g->scale, START_DEG);
    if (g->large) {
        lv_scale_set_label_show(g->scale, true);
    } else {
        lv_scale_set_total_tick_count(g->scale, 28);  // every 10°, like the web's 5°/15° at a smaller size
        lv_scale_set_major_tick_every(g->scale, 3);
        lv_scale_set_label_show(g->scale, false);
    }
    lv_obj_set_style_arc_width(g->scale, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, lv_color_hex(0xCCCCCC), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g->scale, 2, LV_PART_INDICATOR);
    lv_obj_set_style_length(g->scale, g->large ? 16 : 9, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(g->scale, lv_color_hex(0x999999), LV_PART_ITEMS);
    lv_obj_set_style_line_width(g->scale, 1, LV_PART_ITEMS);
    lv_obj_set_style_length(g->scale, g->large ? 8 : 5, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g->scale, lv_color_hex(UI_COLOR_TEXT), LV_PART_INDICATOR);
    lv_obj_set_style_text_font(g->scale, &lv_font_montserrat_20, LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(g->scale, 8, LV_PART_INDICATOR);
    relabel(g);

    // Gauge name just below the pivot, then the value readout.
    lv_obj_t *name = lv_label_create(g->cont);
    lv_label_set_text(name, cfg->name);
    lv_obj_set_style_text_color(name, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(name, g->large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, g->large ? 30 : 17);

    g->value_lbl = lv_label_create(g->cont);
    lv_obj_set_style_text_color(g->value_lbl, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(g->value_lbl, g->large ? &lv_font_montserrat_48 : &lv_font_montserrat_20, 0);
    lv_obj_align(g->value_lbl, LV_ALIGN_CENTER, 0, g->large ? 76 : 37);
    lv_label_set_text(g->value_lbl, "--");

    if (g->large) {
        g->unit_lbl = lv_label_create(g->cont);
        lv_obj_set_style_text_color(g->unit_lbl, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(g->unit_lbl, &lv_font_montserrat_20, 0);
        lv_obj_align(g->unit_lbl, LV_ALIGN_CENTER, 0, 118);
        lv_label_set_text(g->unit_lbl, g->unit);
    }

    // Needle from the pivot toward the arc, plus a hub.
    g->pts[0].x = c;
    g->pts[0].y = c;
    g->pts[1] = g->pts[0];
    g->needle = lv_line_create(g->cont);
    lv_obj_set_size(g->needle, size, size);
    lv_obj_set_pos(g->needle, 0, 0);
    lv_line_set_points(g->needle, g->pts, 2);
    lv_obj_set_style_line_color(g->needle, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_line_width(g->needle, g->large ? 6 : 4, 0);
    lv_obj_set_style_line_rounded(g->needle, true, 0);

    lv_obj_t *hub = lv_obj_create(g->cont);
    lv_obj_remove_style_all(hub);
    const int32_t hub_d = g->large ? 18 : 10;
    lv_obj_set_size(hub, hub_d, hub_d);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(UI_COLOR_TEXT), 0);

    g->shown_pct = 0.0f;
    ui_gauge_set_value(g, NAN);
}

void ui_gauge_set_range(ui_gauge_t *g, float min, float max, const char *unit, uint8_t major_ticks)
{
    if (major_ticks == 0 || major_ticks >= UI_GAUGE_MAX_LABELS) {
        major_ticks = g->major_ticks;
    }
    if (g->min == min && g->max == max && g->major_ticks == major_ticks && strcmp(g->unit, unit) == 0) {
        return;
    }
    g->major_ticks = major_ticks;
    g->min = min;
    g->max = max;
    snprintf(g->unit, sizeof(g->unit), "%s", unit);
    if (g->unit_lbl) {
        lv_label_set_text(g->unit_lbl, g->unit);
    }
    relabel(g);
}

void ui_gauge_set_value(ui_gauge_t *g, float value)
{
    float target = 0.0f;
    if (isfinite(value)) {
        // Supports inverted scales (min > max), e.g. the current gauge.
        target = (value - g->min) / (g->max - g->min);
        if (target < 0.0f) target = 0.0f;
        if (target > 1.0f) target = 1.0f;
    }
    g->shown_pct += (target - g->shown_pct) * EASE;
    if (fabsf(target - g->shown_pct) < 0.001f) {
        g->shown_pct = target;
    }

    const float pct = g->shown_pct;
    lv_arc_set_value(g->arc, (int32_t)lroundf(pct * SCALE_MAX));
    // Same cue as the web dash: arc turns red within the first 5 % of the sweep.
    lv_obj_set_style_arc_color(g->arc, lv_color_hex(target < 0.05f ? UI_COLOR_RED : UI_COLOR_ARC),
                               LV_PART_INDICATOR);

    const float rad = (START_DEG + pct * SWEEP_DEG) * (float)M_PI / 180.0f;
    const float len = (float)(g->radius - arc_width(g) - (g->large ? 10 : 4));
    const float c = (float)(cont_size(g) / 2);
    g->pts[1].x = c + len * cosf(rad);
    g->pts[1].y = c + len * sinf(rad);
    lv_line_set_points(g->needle, g->pts, 2);

    char buf[16];
    if (!isfinite(value)) {
        snprintf(buf, sizeof(buf), g->large ? "--" : "-- %s", g->unit);
    } else if (g->large) {
        snprintf(buf, sizeof(buf), "%ld", lroundf(value));
    } else {
        snprintf(buf, sizeof(buf), "%.*f %s", g->decimals, value, g->unit);
    }
    lv_label_set_text(g->value_lbl, buf);
}
