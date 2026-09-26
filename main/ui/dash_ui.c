#include "dash_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dash_calc.h"
#include "dash_model.h"
#include "ui_gauge.h"
#include "ui_theme.h"

#define SCREEN_W      800
#define SCREEN_H      800
#define REFRESH_MS    50    // gauges ease toward new values at this rate
#define BLINK_MS      350   // critical warnings: 0.7 s on/off cycle, as on the web dash

static struct {
    lv_obj_t *tileview;
    lv_obj_t *tile_driver;
    lv_obj_t *tile_service;

    // Driver page
    ui_gauge_t speed, voltage, current, aux12v, inv_temp, motor_temp;
    lv_obj_t *dir_lbl[3];  // R, N, F
    lv_obj_t *fault_lbl;
    lv_obj_t *mode_icon, *mode_lbl, *plug_lbl;
    lv_obj_t *soc_bar, *soc_lbl, *range_lbl, *eta_lbl, *plug_icon;
    lv_obj_t *odo_lbl, *odo_unit_lbl;
    lv_obj_t *low_banner;

    // Service page
    lv_obj_t *btn_speed, *btn_temp, *btn_odo;
    lv_obj_t *v_odo, *v_voltage, *v_current, *v_aux, *v_raw31a, *v_inv, *v_motor, *v_soc,
             *v_mode, *v_dir, *v_plug, *v_obc, *v_err, *v_can, *v_frames, *v_last;

    bool blink_on;
    bool soc_critical;
    bool fault;
} ui;

// ── helpers ────────────────────────────────────────────────────────────────

static void set_text(lv_obj_t *lbl, const char *txt)
{
    // Only touch labels whose text changed, so LVGL doesn't redraw them 20x a second.
    if (strcmp(lv_label_get_text(lbl), txt) != 0) {
        lv_label_set_text(lbl, txt);
    }
}

// Style setters that do nothing when the value is unchanged. Re-applying a style
// makes LVGL recompute layout and redraw, and doing that 20x a second for every
// label interrupted the page-swipe animation, so only touch what changed.
static void set_text_color(lv_obj_t *o, uint32_t hex, lv_style_selector_t part)
{
    lv_color_t c = lv_color_hex(hex);
    if (!lv_color_eq(lv_obj_get_style_text_color(o, part), c)) {
        lv_obj_set_style_text_color(o, c, part);
    }
}

static void set_bg_color(lv_obj_t *o, uint32_t hex, lv_style_selector_t part)
{
    lv_color_t c = lv_color_hex(hex);
    if (!lv_color_eq(lv_obj_get_style_bg_color(o, part), c)) {
        lv_obj_set_style_bg_color(o, c, part);
    }
}

static void set_font(lv_obj_t *o, const lv_font_t *font)
{
    if (lv_obj_get_style_text_font(o, 0) != font) {
        lv_obj_set_style_text_font(o, font, 0);
    }
}

static void set_opa(lv_obj_t *o, lv_opa_t opa)
{
    if (lv_obj_get_style_opa(o, 0) != opa) {
        lv_obj_set_style_opa(o, opa, 0);
    }
}

static void set_shadow_width(lv_obj_t *o, int32_t w)
{
    if (lv_obj_get_style_shadow_width(o, LV_PART_MAIN) != w) {
        lv_obj_set_style_shadow_width(o, w, LV_PART_MAIN);
    }
}

static void set_hidden(lv_obj_t *o, bool hidden)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) != hidden) {
        if (hidden) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// "12.3 V", or "-- V" while the value hasn't been received yet.
static void fmt_value(char *buf, size_t n, float v, int decimals, const char *unit)
{
    if (isfinite(v)) {
        snprintf(buf, n, "%.*f %s", decimals, v, unit);
    } else {
        snprintf(buf, n, "-- %s", unit);
    }
}

static void fmt_temp(char *buf, size_t n, float c, const dash_settings_t *cfg)
{
    if (isfinite(c)) {
        snprintf(buf, n, "%.1f %s", dash_temp(c, cfg), cfg->use_fahrenheit ? "°F" : "°C");
    } else {
        snprintf(buf, n, "-- %s", cfg->use_fahrenheit ? "°F" : "°C");
    }
}

static void fmt_thousands(char *buf, size_t n, long v)
{
    char digits[24];
    snprintf(digits, sizeof(digits), "%ld", v);
    size_t len = strlen(digits), o = 0;
    for (size_t i = 0; i < len && o + 2 < n; i++) {
        buf[o++] = digits[i];
        size_t left = len - i - 1;
        if (left > 0 && left % 3 == 0) {
            buf[o++] = ',';
        }
    }
    buf[o] = '\0';
}

static void on_nav_clicked(lv_event_t *e)
{
    dash_ui_show_page((int)(intptr_t)lv_event_get_user_data(e));
}

// Tappable page arrow at the screen edge. The hit area is much larger than the
// glyph so it's easy to hit on the move; swiping between pages also works.
static void nav_button(lv_obj_t *tile, const char *symbol, lv_align_t align, int x_ofs, int page)
{
    lv_obj_t *btn = plain_obj(tile);
    lv_obj_set_size(btn, 72, 220);  // narrow enough to stay clear of the side gauges
    lv_obj_align(btn, align, x_ofs, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_nav_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)page);
    // Text colour is inherited by the arrow; brighten it while pressed so the tap registers.
    lv_obj_set_style_text_color(btn, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(UI_COLOR_ACCENT), LV_STATE_PRESSED);

    lv_obj_t *arrow = lv_label_create(btn);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_40, 0);
    lv_label_set_text(arrow, symbol);
    lv_obj_center(arrow);
}

// ── driver page ────────────────────────────────────────────────────────────

static void build_driver(lv_obj_t *t)
{
    lv_obj_t *mark = label(t, &lv_font_montserrat_20, UI_COLOR_WORDMARK, "ANGRY PIXIE GARAGE");
    lv_obj_set_style_text_letter_space(mark, 5, 0);
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 52);

    const ui_gauge_cfg_t speed = {
        .name = "Speed", .unit = "kph", .min = 0, .max = 160, .radius = 175,
        .large = true, .major_ticks = 8,
    };
    ui_gauge_create(&ui.speed, t, &speed, 400, 290);

    // Small gauges: Voltage + 12V on the left, Inverter + Motor on the right,
    // the (larger) current gauge bottom-centre — same grouping as the web dash.
    const ui_gauge_cfg_t voltage = { .name = "Voltage", .unit = "V", .min = 0, .max = 450, .radius = 62 };
    const ui_gauge_cfg_t aux = {
        .name = "12V Batt", .unit = "V", .min = 10, .max = 16, .radius = 62, .decimals = 1,
        .zones = { { 0.0f, 0.25f, UI_COLOR_RED }, { 0.8333f, 1.0f, UI_COLOR_RED } },  // <11.5 V, >15 V
        .zone_count = 2,
    };
    const ui_gauge_cfg_t inv = {
        .name = "Inverter", .unit = "°C", .min = 0, .max = 90, .radius = 62,
        .zones = { { 0.8f, 1.0f, UI_COLOR_RED } }, .zone_count = 1,
    };
    ui_gauge_cfg_t motor = inv;
    motor.name = "Motor";
    // Inverted: +150 A regen at the start, -250 A throttle at the end, so
    // throttle swings the needle clockwise. Green = regen, red = 200-250 A.
    const ui_gauge_cfg_t current = {
        .name = "Current", .unit = "A", .min = 150, .max = -250, .radius = 80,
        .zones = { { 0.0f, 0.375f, UI_COLOR_GREEN_ZONE }, { 0.875f, 1.0f, UI_COLOR_RED } },
        .zone_count = 2,
    };
    ui_gauge_create(&ui.voltage, t, &voltage, 138, 262);
    ui_gauge_create(&ui.aux12v, t, &aux, 138, 470);
    ui_gauge_create(&ui.inv_temp, t, &inv, 662, 262);
    ui_gauge_create(&ui.motor_temp, t, &motor, 662, 470);
    ui_gauge_create(&ui.current, t, &current, 400, 592);

    // R N F in the speed gauge's open bottom.
    lv_obj_t *dir = plain_obj(t);
    lv_obj_set_size(dir, 220, 52);
    lv_obj_align(dir, LV_ALIGN_TOP_MID, 0, 420);
    lv_obj_set_flex_flow(dir, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dir, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(dir, 26, 0);
    static const char *const dir_txt[3] = { "R", "N", "F" };
    for (int i = 0; i < 3; i++) {
        ui.dir_lbl[i] = label(dir, &lv_font_montserrat_28, UI_COLOR_INACTIVE, dir_txt[i]);
    }

    ui.fault_lbl = label(t, &lv_font_montserrat_20, UI_COLOR_RED, LV_SYMBOL_WARNING " INVERTER FAULT");
    lv_obj_align(ui.fault_lbl, LV_ALIGN_TOP_MID, 0, 478);
    lv_obj_add_flag(ui.fault_lbl, LV_OBJ_FLAG_HIDDEN);

    // Right of the current gauge: op mode + plug status (the web status box).
    lv_obj_t *mode = plain_obj(t);
    lv_obj_set_size(mode, 190, 90);
    lv_obj_set_pos(mode, 505, 560);
    lv_obj_set_flex_flow(mode, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mode, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(mode, 4, 0);
    lv_obj_t *mode_row = plain_obj(mode);
    lv_obj_set_size(mode_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mode_row, 8, 0);
    ui.mode_icon = label(mode_row, &lv_font_montserrat_24, UI_COLOR_TEXT_DIM, LV_SYMBOL_POWER);
    ui.mode_lbl = label(mode_row, &lv_font_montserrat_24, UI_COLOR_TEXT, "--");
    ui.plug_lbl = label(mode, &lv_font_montserrat_16, UI_COLOR_TEXT_MUTED, "");

    // Left of the current gauge: odometer.
    lv_obj_t *odo = plain_obj(t);
    lv_obj_set_size(odo, 200, 90);
    lv_obj_set_pos(odo, 100, 560);
    lv_obj_set_flex_flow(odo, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(odo, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(odo, 4, 0);
    lv_obj_t *odo_cap = label(odo, &lv_font_montserrat_14, UI_COLOR_TEXT_MUTED, "ODO");
    lv_obj_set_style_text_letter_space(odo_cap, 3, 0);
    lv_obj_t *odo_row = plain_obj(odo);
    lv_obj_set_size(odo_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(odo_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(odo_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(odo_row, 6, 0);
    ui.odo_lbl = label(odo_row, &lv_font_montserrat_24, 0xEEEEEE, "------");
    lv_obj_set_style_bg_color(ui.odo_lbl, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(ui.odo_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui.odo_lbl, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(ui.odo_lbl, 1, 0);
    lv_obj_set_style_radius(ui.odo_lbl, 5, 0);
    lv_obj_set_style_pad_hor(ui.odo_lbl, 8, 0);
    lv_obj_set_style_pad_ver(ui.odo_lbl, 2, 0);
    ui.odo_unit_lbl = label(odo_row, &lv_font_montserrat_16, UI_COLOR_INFO, "mi");

    // State of charge bar with % and range underneath.
    ui.plug_icon = label(t, &lv_font_montserrat_24, UI_COLOR_INACTIVE, LV_SYMBOL_CHARGE);
    lv_obj_align(ui.plug_icon, LV_ALIGN_TOP_MID, -228, 686);
    ui.soc_bar = lv_bar_create(t);
    lv_obj_set_size(ui.soc_bar, 420, 28);
    lv_obj_align(ui.soc_bar, LV_ALIGN_TOP_MID, 0, 684);
    lv_bar_set_range(ui.soc_bar, 0, 100);
    lv_obj_set_style_bg_color(ui.soc_bar, lv_color_hex(UI_COLOR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.soc_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.soc_bar, 14, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.soc_bar, 14, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui.soc_bar, lv_color_hex(UI_COLOR_SOC_OK), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(ui.soc_bar, lv_color_hex(UI_COLOR_RED), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.soc_bar, 0, LV_PART_MAIN);

    ui.soc_lbl = label(t, &lv_font_montserrat_20, 0xCCCCCC, "--%");
    lv_obj_align(ui.soc_lbl, LV_ALIGN_TOP_LEFT, 196, 718);
    ui.range_lbl = label(t, &lv_font_montserrat_20, UI_COLOR_INFO, "-- mi");
    lv_obj_align(ui.range_lbl, LV_ALIGN_TOP_RIGHT, -196, 718);
    ui.eta_lbl = label(t, &lv_font_montserrat_16, UI_COLOR_INFO, "");
    lv_obj_align(ui.eta_lbl, LV_ALIGN_TOP_MID, 0, 746);
    lv_obj_add_flag(ui.eta_lbl, LV_OBJ_FLAG_HIDDEN);

    nav_button(t, LV_SYMBOL_RIGHT, LV_ALIGN_RIGHT_MID, 0, 1);  // → service page

    // Critical low-battery banner (SOC < 10 %): impossible to miss.
    ui.low_banner = plain_obj(t);
    lv_obj_set_size(ui.low_banner, SCREEN_W, 96);
    lv_obj_set_pos(ui.low_banner, 0, 118);
    lv_obj_set_style_bg_opa(ui.low_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui.low_banner, lv_color_hex(UI_COLOR_WARN_BG), 0);
    lv_obj_set_style_border_side(ui.low_banner, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(ui.low_banner, 3, 0);
    lv_obj_set_style_border_color(ui.low_banner, lv_color_hex(0xFF6B6B), 0);
    lv_obj_t *big = label(ui.low_banner, &lv_font_montserrat_28, UI_COLOR_TEXT,
                          LV_SYMBOL_WARNING " BATTERY CRITICALLY LOW " LV_SYMBOL_WARNING);
    lv_obj_align(big, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_t *sub = label(ui.low_banner, &lv_font_montserrat_16, UI_COLOR_TEXT,
                          "Charge vehicle immediately - do not proceed");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_add_flag(ui.low_banner, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_driver(const dash_snapshot_t *snap)
{
    const dash_state_t *s = &snap->vehicle;
    const dash_settings_t *cfg = &snap->settings;
    char buf[64];

    // Speed (kph 0-160 in 20s, mph 0-120 in 10s)
    ui_gauge_set_range(&ui.speed, 0, dash_speed_max(cfg), cfg->use_mph ? "mph" : "kph",
                       cfg->use_mph ? 12 : 8);
    ui_gauge_set_value(&ui.speed, dash_speed(s, cfg));

    const char *tu = cfg->use_fahrenheit ? "°F" : "°C";
    ui_gauge_set_range(&ui.inv_temp, 0, dash_temp_max(cfg), tu, 0);
    ui_gauge_set_range(&ui.motor_temp, 0, dash_temp_max(cfg), tu, 0);
    ui_gauge_set_value(&ui.voltage, s->voltage_v);
    ui_gauge_set_value(&ui.current, s->current_a);
    ui_gauge_set_value(&ui.aux12v, s->aux_12v);   // NAN → "--" until CAN data arrives
    ui_gauge_set_value(&ui.inv_temp, dash_temp(s->inv_temp_c, cfg));
    ui_gauge_set_value(&ui.motor_temp, dash_temp(s->motor_temp_c, cfg));

    // Drive direction: -1 R, 0 N, 1 F
    int active = s->drive_dir < 0 ? 0 : (s->drive_dir > 0 ? 2 : 1);
    for (int i = 0; i < 3; i++) {
        bool on = (i == active);
        set_text_color(ui.dir_lbl[i], on ? UI_COLOR_ACCENT : UI_COLOR_INACTIVE, 0);
        set_font(ui.dir_lbl[i], on ? &lv_font_montserrat_48 : &lv_font_montserrat_28);
    }

    // Op mode
    static const struct { const char *icon; uint32_t color; } mode_style[] = {
        [OPMODE_OFF]            = { LV_SYMBOL_POWER,   UI_COLOR_RED },
        [OPMODE_RUN]            = { LV_SYMBOL_CHARGE,  UI_COLOR_SOC_OK },
        [OPMODE_PRECHARGE]      = { LV_SYMBOL_REFRESH, UI_COLOR_PLUG },
        [OPMODE_PRECHARGE_FAIL] = { LV_SYMBOL_WARNING, UI_COLOR_RED },
        [OPMODE_CHARGING]       = { LV_SYMBOL_BATTERY_3, UI_COLOR_PLUG },
    };
    uint8_t m = s->opmode <= OPMODE_CHARGING ? s->opmode : OPMODE_OFF;
    set_text(ui.mode_icon, mode_style[m].icon);
    set_text_color(ui.mode_icon, mode_style[m].color, 0);
    set_text(ui.mode_lbl, dash_opmode_text(s->opmode));
    snprintf(buf, sizeof(buf), "Plug: %s\nOBC: %s", s->plug_inserted ? "Inserted" : "Not inserted",
             dash_obc_text(s->obc_volt_stat));
    set_text(ui.plug_lbl, buf);
    set_text_color(ui.plug_icon, s->plug_inserted ? UI_COLOR_PLUG : UI_COLOR_INACTIVE, 0);

    ui.fault = s->inverter_error;
    set_hidden(ui.fault_lbl, !ui.fault);
    if (!ui.fault) {
        set_opa(ui.fault_lbl, LV_OPA_COVER);  // don't reappear mid-blink at 20 %
    }

    // State of charge, range, low-battery warning
    float soc = s->soc_pct;
    bool soc_known = isfinite(soc);
    lv_bar_set_value(ui.soc_bar, soc_known ? (int32_t)lroundf(soc < 0 ? 0 : (soc > 100 ? 100 : soc)) : 0,
                     LV_ANIM_OFF);
    ui.soc_critical = dash_soc_critical(s);
    set_bg_color(ui.soc_bar, soc_known && soc < DASH_WARN_SOC_PCT ? UI_COLOR_RED : UI_COLOR_SOC_OK,
                 LV_PART_INDICATOR);
    if (ui.soc_critical) {
        // Critically low: don't show the number — flash red and warn.
        set_text(ui.soc_lbl, "LOW");
        set_text_color(ui.soc_lbl, 0xFF2A2A, 0);
        set_shadow_width(ui.soc_bar, 14);
        set_hidden(ui.low_banner, false);
    } else {
        if (soc_known) {
            snprintf(buf, sizeof(buf), "%ld%%", lroundf(soc));
        } else {
            snprintf(buf, sizeof(buf), "--%%");
        }
        set_text(ui.soc_lbl, buf);
        set_text_color(ui.soc_lbl, 0xCCCCCC, 0);
        set_shadow_width(ui.soc_bar, 0);
        set_hidden(ui.low_banner, true);
        set_opa(ui.soc_lbl, LV_OPA_COVER);
        set_opa(ui.soc_bar, LV_OPA_COVER);
    }
    float range = dash_range(s, cfg);
    if (isfinite(range)) {
        snprintf(buf, sizeof(buf), "~%ld %s", lroundf(range), cfg->use_mph ? "mi" : "km");
    } else {
        snprintf(buf, sizeof(buf), "-- %s", cfg->use_mph ? "mi" : "km");
    }
    set_text(ui.range_lbl, buf);

    int eta = dash_charge_eta_min(s);
    if (eta >= 0) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " ~%dh %02dm to full", eta / 60, eta % 60);
        set_text(ui.eta_lbl, buf);
        set_hidden(ui.eta_lbl, false);
    } else {
        set_hidden(ui.eta_lbl, true);
    }

    // Odometer
    fmt_thousands(buf, sizeof(buf), (long)floor(dash_odo_display(snap->odo_m, cfg)));
    set_text(ui.odo_lbl, buf);
    set_text(ui.odo_unit_lbl, cfg->use_odo_miles ? "mi" : "km");
}

// ── service page ───────────────────────────────────────────────────────────

static const char *const map_speed[] = { "KPH", "MPH", "" };
static const char *const map_temp[] = { "°C", "°F", "" };
static const char *const map_odo[] = { "KM", "MILES", "" };

static void on_unit_changed(lv_event_t *e)
{
    (void)e;
    dash_snapshot_t snap;
    dash_model_snapshot(&snap);
    dash_settings_t cfg = snap.settings;
    cfg.use_mph = lv_buttonmatrix_get_selected_button(ui.btn_speed) == 1;
    cfg.use_fahrenheit = lv_buttonmatrix_get_selected_button(ui.btn_temp) == 1;
    cfg.use_odo_miles = lv_buttonmatrix_get_selected_button(ui.btn_odo) == 1;
    dash_model_set_settings(&cfg);
}

static lv_obj_t *toggle_row(lv_obj_t *col, const char *name, const char *const *map)
{
    lv_obj_t *row = plain_obj(col);
    lv_obj_set_size(row, LV_PCT(100), 56);
    label(row, &lv_font_montserrat_20, UI_COLOR_TEXT, name);
    lv_obj_align(lv_obj_get_child(row, 0), LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *bm = lv_buttonmatrix_create(row);
    lv_buttonmatrix_set_map(bm, map);
    lv_buttonmatrix_set_button_ctrl_all(bm, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(bm, true);
    lv_obj_set_size(bm, 240, 52);
    lv_obj_align(bm, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_pad_all(bm, 4, 0);
    lv_obj_set_style_pad_gap(bm, 6, 0);
    lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_border_color(bm, lv_color_hex(UI_COLOR_INACTIVE), LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 2, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, lv_color_hex(UI_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(bm, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x0060FF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(bm, lv_color_hex(0x0060FF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(bm, on_unit_changed, LV_EVENT_VALUE_CHANGED, NULL);
    return bm;
}

static lv_obj_t *data_row(lv_obj_t *col, const char *name)
{
    lv_obj_t *row = plain_obj(col);
    lv_obj_set_size(row, LV_PCT(100), 30);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0x222222), 0);
    lv_obj_t *n = label(row, &lv_font_montserrat_16, UI_COLOR_TEXT_DIM, name);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = label(row, &lv_font_montserrat_16, UI_COLOR_TEXT, "--");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

static void build_service(lv_obj_t *t)
{
    nav_button(t, LV_SYMBOL_LEFT, LV_ALIGN_LEFT_MID, 0, 0);  // ← driver page

    // Content column kept inside the round glass; scrolls if it doesn't fit.
    lv_obj_t *col = plain_obj(t);
    lv_obj_set_size(col, 540, 660);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    lv_obj_set_style_pad_bottom(col, 120, 0);

    lv_obj_t *title = label(col, &lv_font_montserrat_24, UI_COLOR_TEXT, "Service / Diagnostics");
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(title, 10, 0);

    ui.btn_speed = toggle_row(col, "Speed", map_speed);
    ui.btn_temp = toggle_row(col, "Temperature", map_temp);
    ui.btn_odo = toggle_row(col, "Odometer", map_odo);

    lv_obj_t *gap = plain_obj(col);
    lv_obj_set_size(gap, 10, 10);

    ui.v_odo = data_row(col, "Odometer");
    ui.v_voltage = data_row(col, "DC bus voltage");
    ui.v_current = data_row(col, "Battery current");
    ui.v_aux = data_row(col, "12V battery");
    ui.v_raw31a = data_row(col, "ZombieVerter 0x31A raw");
    ui.v_inv = data_row(col, "Inverter temp");
    ui.v_motor = data_row(col, "Motor temp");
    ui.v_soc = data_row(col, "State of charge");
    ui.v_mode = data_row(col, "Mode");
    ui.v_dir = data_row(col, "Drive direction");
    ui.v_plug = data_row(col, "Plug status");
    ui.v_obc = data_row(col, "OBC status");
    ui.v_err = data_row(col, "Inverter error");
    ui.v_can = data_row(col, "CAN bus");
    ui.v_frames = data_row(col, "Frames rx / decoded");
    ui.v_last = data_row(col, "Last frame");
}

static void sync_toggle(lv_obj_t *bm, bool second)
{
    uint32_t want = second ? 1 : 0;
    if (!lv_buttonmatrix_has_button_ctrl(bm, want, LV_BUTTONMATRIX_CTRL_CHECKED)) {
        lv_buttonmatrix_set_button_ctrl(bm, want, LV_BUTTONMATRIX_CTRL_CHECKED);  // one_checked clears the other
    }
}

static void refresh_service(const dash_snapshot_t *snap)
{
    const dash_state_t *s = &snap->vehicle;
    const dash_settings_t *cfg = &snap->settings;
    char buf[64];

    sync_toggle(ui.btn_speed, cfg->use_mph);
    sync_toggle(ui.btn_temp, cfg->use_fahrenheit);
    sync_toggle(ui.btn_odo, cfg->use_odo_miles);

    char num[24];
    fmt_thousands(num, sizeof(num), (long)floor(dash_odo_display(snap->odo_m, cfg)));
    snprintf(buf, sizeof(buf), "%s %s", num, cfg->use_odo_miles ? "mi" : "km");
    set_text(ui.v_odo, buf);

    fmt_value(buf, sizeof(buf), s->voltage_v, 1, "V");
    set_text(ui.v_voltage, buf);
    fmt_value(buf, sizeof(buf), s->current_a, 0, "A");
    set_text(ui.v_current, buf);
    fmt_value(buf, sizeof(buf), s->aux_12v, 1, "V");
    set_text(ui.v_aux, buf);

    if (s->raw_31a_dlc > 0) {
        const uint8_t *r = s->raw_31a;
        snprintf(buf, sizeof(buf), "%02X %02X %02X %02X %02X %02X %02X %02X",
                 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    } else {
        snprintf(buf, sizeof(buf), "no 0x31A frames received");
    }
    set_text(ui.v_raw31a, buf);

    fmt_temp(buf, sizeof(buf), s->inv_temp_c, cfg);
    set_text(ui.v_inv, buf);
    fmt_temp(buf, sizeof(buf), s->motor_temp_c, cfg);
    set_text(ui.v_motor, buf);
    fmt_value(buf, sizeof(buf), s->soc_pct, 1, "%");
    set_text(ui.v_soc, buf);
    set_text(ui.v_mode, dash_opmode_text(s->opmode));
    set_text(ui.v_dir, s->drive_dir < 0 ? "Reverse" : (s->drive_dir > 0 ? "Forward" : "Neutral"));
    set_text(ui.v_plug, s->plug_inserted ? "Inserted" : "Not inserted");
    set_text(ui.v_obc, dash_obc_text(s->obc_volt_stat));
    set_text(ui.v_err, s->inverter_error ? "FAULT" : "OK");
    set_text_color(ui.v_err, s->inverter_error ? UI_COLOR_RED : UI_COLOR_TEXT, 0);
    set_text(ui.v_can, dash_can_state_text(snap->can_state));
    snprintf(buf, sizeof(buf), "%lu / %lu", (unsigned long)s->frames_rx, (unsigned long)s->frames_decoded);
    set_text(ui.v_frames, buf);
    if (s->last_rx_ms == 0) {
        snprintf(buf, sizeof(buf), "never");
    } else {
        snprintf(buf, sizeof(buf), "%lu ms ago", (unsigned long)(snap->now_ms - s->last_rx_ms));
    }
    set_text(ui.v_last, buf);
}

// ── timers ─────────────────────────────────────────────────────────────────

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    dash_snapshot_t snap;
    dash_model_snapshot(&snap);
    // Only update the page on screen: redrawing the hidden one just steals
    // time from touch handling.
    if (lv_tileview_get_tile_active(ui.tileview) == ui.tile_service) {
        refresh_service(&snap);
    } else {
        refresh_driver(&snap);
    }
}

static void blink_cb(lv_timer_t *t)
{
    (void)t;
    ui.blink_on = !ui.blink_on;
    lv_opa_t opa = ui.blink_on ? LV_OPA_COVER : LV_OPA_20;
    if (ui.soc_critical) {
        lv_obj_set_style_opa(ui.soc_lbl, opa, 0);
        lv_obj_set_style_opa(ui.soc_bar, opa, 0);
        lv_obj_set_style_bg_color(ui.low_banner, lv_color_hex(ui.blink_on ? UI_COLOR_WARN_BG : 0x350000), 0);
    }
    if (ui.fault) {
        lv_obj_set_style_opa(ui.fault_lbl, opa, 0);
    }
}

static void on_tile_changed(lv_event_t *e)
{
    (void)e;
    dash_snapshot_t snap;
    dash_model_snapshot(&snap);
    refresh_driver(&snap);   // no stale values on the page that just slid in
    refresh_service(&snap);
}

void dash_ui_create(lv_obj_t *screen)
{
    memset(&ui, 0, sizeof(ui));
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    ui.tileview = lv_tileview_create(screen);
    lv_obj_set_size(ui.tileview, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(ui.tileview, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_scrollbar_mode(ui.tileview, LV_SCROLLBAR_MODE_OFF);
    ui.tile_driver = lv_tileview_add_tile(ui.tileview, 0, 0, LV_DIR_RIGHT);
    ui.tile_service = lv_tileview_add_tile(ui.tileview, 1, 0, LV_DIR_LEFT);
    lv_obj_add_event_cb(ui.tileview, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

    build_driver(ui.tile_driver);
    build_service(ui.tile_service);

    // Red anodized bezel ring around the round glass, on top of both pages.
    lv_obj_t *bezel = plain_obj(screen);
    lv_obj_set_size(bezel, SCREEN_W - 8, SCREEN_H - 8);
    lv_obj_center(bezel);
    lv_obj_remove_flag(bezel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(bezel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(bezel, 6, 0);
    lv_obj_set_style_border_color(bezel, lv_color_hex(UI_COLOR_BEZEL), 0);
    // A thin darker inner ring instead of a blurred glow: a shadow this size is
    // expensive to draw and was redrawn whenever anything near the edge changed.
    lv_obj_set_style_outline_width(bezel, 0, 0);
    lv_obj_t *inner = plain_obj(screen);
    lv_obj_set_size(inner, SCREEN_W - 20, SCREEN_H - 20);
    lv_obj_center(inner);
    lv_obj_remove_flag(inner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(inner, 2, 0);
    lv_obj_set_style_border_color(inner, lv_color_hex(0x5A0714), 0);

    dash_snapshot_t snap;
    dash_model_snapshot(&snap);
    refresh_driver(&snap);
    refresh_service(&snap);

    lv_timer_create(refresh_cb, REFRESH_MS, NULL);
    lv_timer_create(blink_cb, BLINK_MS, NULL);
}

void dash_ui_show_page(int page)
{
    lv_obj_update_layout(ui.tileview);  // tile positions must be known before scrolling to one
    lv_tileview_set_tile(ui.tileview, page ? ui.tile_service : ui.tile_driver, LV_ANIM_ON);
}
