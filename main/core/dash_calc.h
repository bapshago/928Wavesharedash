// Derived values and display logic that the web dash split between
// handleApiData() and the page JavaScript (range, time-to-full, unit
// conversion, odometer integration, test data). Plain C, host-testable.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dash_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Drivetrain / pack constants carried over from leaf_dash_v9.ino and driverpage.h.
#define DASH_GEAR_RATIO           7.94f
#define DASH_TIRE_CIRCUMFERENCE_M 1.975f
#define DASH_PACK_KWH             22.0f
#define DASH_MI_PER_KWH           3.9f
#define DASH_LOW_SOC_PCT          10.0f   // below this: flashing critical warning
#define DASH_WARN_SOC_PCT         15.0f   // below this: red charge bar
#define DASH_ODO_INITIAL_MILES    150000.0
#define DASH_METERS_PER_MILE      1609.344
#define DASH_KM_PER_MILE          1.60934f

// User settings, persisted to NVS on the device.
typedef struct {
    bool use_mph;         // speed + range in mph/mi instead of kph/km
    bool use_fahrenheit;  // temperatures in °F
    bool use_odo_miles;   // odometer in miles instead of km
} dash_settings_t;

float dash_rpm_to_kph(float rpm);   // NAN if the drivetrain constants are invalid
float dash_kph_to_mph(float kph);
float dash_c_to_f(float c);

// Vehicle speed in the selected unit (0 if unknown).
float dash_speed(const dash_state_t *s, const dash_settings_t *cfg);
// Full-scale value of the speed gauge: 120 mph or 160 kph.
float dash_speed_max(const dash_settings_t *cfg);
// Temperature in the selected unit (NAN stays NAN), and gauge full-scale (200 °F / 90 °C).
float dash_temp(float celsius, const dash_settings_t *cfg);
float dash_temp_max(const dash_settings_t *cfg);

// Estimated range in the selected unit from SOC (NAN until SOC is known).
float dash_range(const dash_state_t *s, const dash_settings_t *cfg);

bool dash_is_charging(const dash_state_t *s);
// Minutes to full while charging, or -1 when not charging / power too low.
int dash_charge_eta_min(const dash_state_t *s);

// SOC below 10 %. False while SOC is still unknown.
bool dash_soc_critical(const dash_state_t *s);

const char *dash_opmode_text(uint8_t opmode);
const char *dash_obc_text(uint8_t obc_volt_stat);

// Odometer: add the distance travelled in dt_ms at the current motor speed.
// Reverse still adds mileage. Returns the new total in metres.
double dash_odo_integrate(double odo_m, float motor_rpm, uint32_t dt_ms);

// Odometer reading in the selected unit (miles or km).
double dash_odo_display(double odo_m, const dash_settings_t *cfg);

// Test mode: every gauge sweeps linearly from the bottom of its range to the
// top and back over DASH_TEST_CYCLE_MS (SOC runs the other way, 100 → 0 %);
// on/off states step through their values every DASH_TEST_STEP_MS.
#define DASH_TEST_CYCLE_MS 20000
#define DASH_TEST_STEP_MS  3000
void dash_test_data(dash_state_t *s, uint32_t t_ms, const dash_settings_t *cfg);

#ifdef __cplusplus
}
#endif
