// Host unit tests for the CAN decoder and dash calculations. No ESP-IDF needed:
//   cmake -S host_test -B build-test && cmake --build build-test && ./build-test/test_core
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dash_calc.h"
#include "leaf_can.h"

static int failures, checks;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        checks++;                                                              \
        double _a = (a), _b = (b);                                             \
        if (!(fabs(_a - _b) <= (eps))) {                                       \
            failures++;                                                        \
            printf("FAIL %s:%d: %s = %g, expected %g\n", __FILE__, __LINE__, #a, _a, _b); \
        }                                                                      \
    } while (0)

static bool decode(dash_state_t *s, uint32_t id, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                   uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
    const uint8_t d[8] = { d0, d1, d2, d3, d4, d5, d6, d7 };
    return leaf_can_decode(id, d, 8, s);
}

static void test_1da_inverter(void)
{
    dash_state_t s;
    dash_state_init(&s);
    // 352.5 V → raw 705 = 0b10_1100_0001 → d0 = 0xB0, d1 top bits = 01
    // 5200 rpm → raw 10400 = 0x28A0
    CHECK(decode(&s, 0x1DA, 0xB0, 0x40, 0, 0, 0x28, 0xA0, 0x00, 0));
    CHECK_NEAR(s.voltage_v, 352.5, 1e-3);
    CHECK_NEAR(s.motor_rpm, 5200, 1e-3);
    CHECK(!s.inverter_error);

    // Reverse: -1000 rpm → raw -2000 = 0xF830; error bit 0x80 set
    decode(&s, 0x1DA, 0xB0, 0x40, 0, 0, 0xF8, 0x30, 0x80, 0);
    CHECK_NEAR(s.motor_rpm, -1000, 1e-3);
    CHECK(s.inverter_error);

    // 0x7FFF = "not available" → 0
    decode(&s, 0x1DA, 0xB0, 0x40, 0, 0, 0x7F, 0xFF, 0x00, 0);
    CHECK_NEAR(s.motor_rpm, 0, 1e-6);

    // Bits outside the 0xB0 error mask don't count as a fault
    decode(&s, 0x1DA, 0xB0, 0x40, 0, 0, 0, 0, 0x4F, 0);
    CHECK(!s.inverter_error);
}

static void test_1db_current(void)
{
    dash_state_t s;
    dash_state_init(&s);
    // raw = d0 << 3 | d1 >> 5 (11-bit two's complement)
    decode(&s, 0x1DB, 0x08, 0x00, 0, 0, 0, 0, 0, 0);  // raw 64
    CHECK_NEAR(s.current_a, 64, 1e-6);
    decode(&s, 0x1DB, 0x7F, 0xE0, 0, 0, 0, 0, 0, 0);  // raw 1023, largest positive
    CHECK_NEAR(s.current_a, 1023, 1e-6);
    decode(&s, 0x1DB, 0x80, 0x00, 0, 0, 0, 0, 0, 0);  // raw 1024 → -1024
    CHECK_NEAR(s.current_a, -1024, 1e-6);
    decode(&s, 0x1DB, 0xFF, 0xE0, 0, 0, 0, 0, 0, 0);  // raw 2047 → -1 (the web dash's -2047 gave 0)
    CHECK_NEAR(s.current_a, -1, 1e-6);
    decode(&s, 0x1DB, 0xF1, 0x40, 0, 0, 0, 0, 0, 0);  // raw 1930 → -118
    CHECK_NEAR(s.current_a, -118, 1e-6);
}

static void test_soc_temps_obc_aux(void)
{
    dash_state_t s;
    dash_state_init(&s);
    CHECK(isnan(s.inv_temp_c) && isnan(s.motor_temp_c) && isnan(s.aux_12v));
    CHECK(isnan(s.soc_pct) && isnan(s.voltage_v) && isnan(s.current_a));
    CHECK(!dash_soc_critical(&s));  // no false low-battery warning before 0x55B arrives

    decode(&s, 0x55B, 0xAB, 0x00, 0, 0, 0, 0, 0, 0);  // raw 684 → 68.4 %
    CHECK_NEAR(s.soc_pct, 68.4, 1e-3);
    decode(&s, 0x55B, 0xFA, 0x00, 0, 0, 0, 0, 0, 0);  // raw 1000 → 100 %
    CHECK_NEAR(s.soc_pct, 100.0, 1e-3);

    decode(&s, 0x55A, 0, 212, 32, 0, 0, 0, 0, 0);  // motor 212 °F, inverter 32 °F
    CHECK_NEAR(s.motor_temp_c, 100.0, 1e-3);
    CHECK_NEAR(s.inv_temp_c, 0.0, 1e-3);

    decode(&s, 0x390, 0, 0, 0, 2 << 3, 0, 0x08, 0, 0);
    CHECK(s.obc_volt_stat == 2);
    CHECK(s.plug_inserted);
    decode(&s, 0x390, 0, 0, 0, 0, 0, 0x04, 0, 0);
    CHECK(s.obc_volt_stat == 0);
    CHECK(!s.plug_inserted);

    decode(&s, 0x292, 0, 0, 0, 0x7F, 0, 0, 0, 0);  // 12.7 V
    CHECK_NEAR(s.aux_12v, 12.7, 1e-3);
}

static void test_31a_zombieverter(void)
{
    dash_state_t s;
    dash_state_init(&s);
    // Example frame from ZombieVerter_CAN_Mappings.md: 01 01 84 00 → Forward, Run, 13.2 V
    CHECK(decode(&s, 0x31A, 0x01, 0x01, 0x84, 0x00, 0, 0, 0, 0));
    CHECK(s.drive_dir == 1);
    CHECK(s.opmode == OPMODE_RUN);
    CHECK_NEAR(s.aux_12v, 13.2, 1e-3);
    CHECK(s.raw_31a_dlc == 8 && s.raw_31a[2] == 0x84);

    // Reverse, charging; uaux bytes zero = mapping missing → keep last value
    decode(&s, 0x31A, 0xFF, 0x04, 0x00, 0x00, 0, 0, 0, 0);
    CHECK(s.drive_dir == -1);
    CHECK(s.opmode == OPMODE_CHARGING);
    CHECK_NEAR(s.aux_12v, 13.2, 1e-3);

    // Unexpected values fall back to neutral / off
    decode(&s, 0x31A, 0x05, 0x09, 0x10, 0x01, 0, 0, 0, 0);  // uaux 0x0110 = 27.2 V
    CHECK(s.drive_dir == 0);
    CHECK(s.opmode == OPMODE_OFF);
    CHECK_NEAR(s.aux_12v, 27.2, 1e-3);
}

static void test_ignored_frames(void)
{
    dash_state_t s;
    dash_state_init(&s);
    const uint8_t d[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    CHECK(!leaf_can_decode(0x55B, d, 5, &s));  // shorter than 6 bytes
    CHECK(isnan(s.soc_pct));
    CHECK(!leaf_can_decode(0x123, d, 8, &s));  // unknown ID
}

static void test_calc(void)
{
    dash_settings_t kph = { 0 }, mph = { .use_mph = true, .use_fahrenheit = true, .use_odo_miles = true };
    dash_state_t s;
    dash_state_init(&s);

    // 1000 rpm / 7.94 * 1.975 m * 60 = 14.924 kph
    CHECK_NEAR(dash_rpm_to_kph(1000), 14.924, 0.01);
    s.motor_rpm = 5200;
    CHECK_NEAR(dash_speed(&s, &kph), 77.6, 0.1);
    CHECK_NEAR(dash_speed(&s, &mph), 48.2, 0.1);
    s.motor_rpm = -5200;  // reverse shows as negative speed, same as the web dash
    CHECK(dash_speed(&s, &kph) < 0);

    CHECK_NEAR(dash_speed_max(&kph), 160, 0);
    CHECK_NEAR(dash_speed_max(&mph), 120, 0);
    CHECK_NEAR(dash_temp(100, &mph), 212, 1e-3);
    CHECK_NEAR(dash_temp(100, &kph), 100, 1e-3);
    CHECK(isnan(dash_temp(NAN, &mph)));

    // Range: 22 kWh * 3.9 mi/kWh * SOC
    s.soc_pct = 50;
    CHECK_NEAR(dash_range(&s, &mph), 42.9, 0.01);
    CHECK_NEAR(dash_range(&s, &kph), 69.04, 0.01);

    // Low battery threshold
    s.soc_pct = 9.9f;
    CHECK(dash_soc_critical(&s));
    s.soc_pct = 10.0f;
    CHECK(!dash_soc_critical(&s));

    // Time to full: 364 V * 18 A = 6.552 kW, 12.98 kWh left → 119 min
    s.soc_pct = 41;
    s.voltage_v = 364;
    s.current_a = 18;
    s.opmode = OPMODE_RUN;
    s.obc_volt_stat = 0;
    CHECK(dash_charge_eta_min(&s) == -1);  // not charging
    s.opmode = OPMODE_CHARGING;
    CHECK(dash_charge_eta_min(&s) == 119);
    s.opmode = OPMODE_RUN;
    s.obc_volt_stat = 2;  // OBC says charging is also enough
    CHECK(dash_charge_eta_min(&s) == 119);
    s.current_a = 0.1f;  // under 0.2 kW → no estimate
    CHECK(dash_charge_eta_min(&s) == -1);
    s.current_a = NAN;  // current not received yet → no estimate
    CHECK(dash_charge_eta_min(&s) == -1);
    s.soc_pct = NAN;
    CHECK(isnan(dash_range(&s, &mph)));

    CHECK(strcmp(dash_opmode_text(OPMODE_PRECHARGE_FAIL), "Pre Charge Failed") == 0);
    CHECK(strcmp(dash_opmode_text(99), "Unknown") == 0);
    CHECK(strcmp(dash_obc_text(1), "AC Present") == 0);

    // Odometer: 1 hour at 5200 rpm ≈ 77.6 km, reverse counts too
    double odo = 0;
    for (int i = 0; i < 3600 * 4; i++) {
        odo = dash_odo_integrate(odo, (i % 2) ? 5200 : -5200, 250);
    }
    CHECK_NEAR(odo / 1000.0, 77.6, 0.1);
    CHECK_NEAR(dash_odo_display(150000 * DASH_METERS_PER_MILE, &mph), 150000, 1e-6);
    CHECK_NEAR(dash_odo_display(1234567, &kph), 1234.567, 1e-6);
}

int main(void)
{
    test_1da_inverter();
    test_1db_current();
    test_soc_temps_obc_aux();
    test_31a_zombieverter();
    test_ignored_frames();
    test_calc();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
