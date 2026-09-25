#include "dash_calc.h"
#include "dash_model.h"

#include <math.h>

float dash_rpm_to_kph(float rpm)
{
    if (DASH_GEAR_RATIO <= 0.01f || DASH_TIRE_CIRCUMFERENCE_M <= 0.01f) {
        return NAN;
    }
    return rpm * (DASH_TIRE_CIRCUMFERENCE_M / 60.0f) * (3.6f / DASH_GEAR_RATIO);
}

float dash_kph_to_mph(float kph) { return kph * 0.621371f; }

float dash_c_to_f(float c) { return c * 9.0f / 5.0f + 32.0f; }

float dash_speed(const dash_state_t *s, const dash_settings_t *cfg)
{
    float kph = dash_rpm_to_kph(s->motor_rpm);
    if (!isfinite(kph)) {
        return 0.0f;
    }
    return cfg->use_mph ? dash_kph_to_mph(kph) : kph;
}

float dash_speed_max(const dash_settings_t *cfg) { return cfg->use_mph ? 120.0f : 160.0f; }

float dash_temp(float celsius, const dash_settings_t *cfg)
{
    return cfg->use_fahrenheit ? dash_c_to_f(celsius) : celsius;
}

float dash_temp_max(const dash_settings_t *cfg) { return cfg->use_fahrenheit ? 200.0f : 90.0f; }

float dash_range(const dash_state_t *s, const dash_settings_t *cfg)
{
    if (!isfinite(s->soc_pct)) {
        return NAN;
    }
    float miles = (s->soc_pct / 100.0f) * DASH_PACK_KWH * DASH_MI_PER_KWH;
    return cfg->use_mph ? miles : miles * DASH_KM_PER_MILE;
}

bool dash_is_charging(const dash_state_t *s)
{
    return s->opmode == OPMODE_CHARGING || s->obc_volt_stat == 2;
}

int dash_charge_eta_min(const dash_state_t *s)
{
    if (!dash_is_charging(s) || !isfinite(s->soc_pct) || !isfinite(s->voltage_v) || !isfinite(s->current_a)) {
        return -1;
    }
    // Charge power (kW) = pack voltage x charge current; hours = kWh still needed / kW.
    float power_kw = fabsf(s->voltage_v * s->current_a) / 1000.0f;
    float remain_kwh = (1.0f - s->soc_pct / 100.0f) * DASH_PACK_KWH;
    if (remain_kwh < 0.0f) {
        remain_kwh = 0.0f;
    }
    if (power_kw <= 0.2f || remain_kwh <= 0.05f) {
        return -1;
    }
    return (int)lroundf(remain_kwh / power_kw * 60.0f);
}

bool dash_soc_critical(const dash_state_t *s)
{
    return isfinite(s->soc_pct) && s->soc_pct < DASH_LOW_SOC_PCT;
}

const char *dash_opmode_text(uint8_t opmode)
{
    switch (opmode) {
    case OPMODE_OFF:            return "Off";
    case OPMODE_RUN:            return "Run";
    case OPMODE_PRECHARGE:      return "Pre Charge";
    case OPMODE_PRECHARGE_FAIL: return "Pre Charge Failed";
    case OPMODE_CHARGING:       return "Charging";
    default:                    return "Unknown";
    }
}

const char *dash_obc_text(uint8_t s)
{
    switch (s) {
    case 0:  return "Idle/No AC";
    case 1:  return "AC Present";
    case 2:  return "Charging";
    case 3:  return "Active/Other";
    default: return "Unknown";
    }
}

double dash_odo_integrate(double odo_m, float motor_rpm, uint32_t dt_ms)
{
    float kph = dash_rpm_to_kph(motor_rpm);
    if (!isfinite(kph)) {
        return odo_m;
    }
    // metres = speed(m/s) * seconds
    return odo_m + (double)fabsf(kph) * (1000.0 / 3600.0) * (dt_ms / 1000.0);
}

double dash_odo_display(double odo_m, const dash_settings_t *cfg)
{
    return cfg->use_odo_miles ? odo_m / DASH_METERS_PER_MILE : odo_m / 1000.0;
}

void dash_test_data(dash_state_t *s, uint32_t t)
{
    s->voltage_v = 215 + (int)(215 * sin(t / 1000.0));
    s->motor_rpm = 4500 + (int)(4500 * sin(t / 800.0));
    s->inverter_error = (t / 5000) % 2 == 0;

    s->inv_temp_c = 45 + (int)(45 * sin(t / 1200.0));
    s->motor_temp_c = 45 + (int)(25 * sin(t / 1000.0));

    s->obc_volt_stat = (t / 3000) % 4;
    s->plug_stat = ((t / 4000) % 2 == 0) ? 0x08 : 0x00;
    s->plug_inserted = (s->plug_stat == 0x08);
    s->soc_pct = 50.0f + 50.0f * sinf(t / 800.0f);
    s->current_a = (int)(80 * sin(t / 800.0));
    s->drive_dir = (int8_t)(int)(2 * sin(t / 800.0));
    s->opmode = (uint8_t)(2 + (int)(2 * sin(t / 800.0)));
    s->aux_12v = 13.2f + 2.2f * sinf(t / 1100.0f);  // sweeps 11.0-15.4 V
}

const char *dash_can_state_text(dash_can_state_t st)
{
    switch (st) {
    case DASH_CAN_OK:        return "OK";
    case DASH_CAN_WARNING:   return "Error warning";
    case DASH_CAN_PASSIVE:   return "Error passive";
    case DASH_CAN_BUS_OFF:   return "BUS OFF";
    case DASH_CAN_TEST_MODE: return "Test mode (fake data)";
    default:                 return "Driver not started";
    }
}
