// Desktop stand-in for main/dash_model.c: no CAN hardware or NVS. Scenarios
// are built from synthetic CAN frames fed through the real decoder, so the
// screenshots exercise the same path as the car.
#include "sim_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "leaf_can.h"

static dash_snapshot_t g_snap;
static bool g_animated;

// ── frame builders (inverse of leaf_can_decode) ────────────────────────────

static void feed(uint32_t id, const uint8_t d[8])
{
    dash_state_t *s = &g_snap.vehicle;
    s->frames_rx++;
    if (leaf_can_decode(id, d, 8, s)) {
        s->frames_decoded++;
    }
    s->last_rx_ms = g_snap.now_ms > 20 ? g_snap.now_ms - 20 : 1;
}

static void tx_1da(float volts, float rpm, bool fault)
{
    int v = (int)lroundf(volts / 0.5f);
    int16_t r = (int16_t)lroundf(rpm / 0.5f);
    uint8_t d[8] = { (uint8_t)(v >> 2), (uint8_t)((v & 3) << 6), 0, 0,
                     (uint8_t)((uint16_t)r >> 8), (uint8_t)r, fault ? 0x80 : 0, 0 };
    feed(0x1DA, d);
}

static void tx_1db(int amps)
{
    uint16_t raw = (uint16_t)amps & 0x7FF;  // 11-bit two's complement
    uint8_t d[8] = { (uint8_t)(raw >> 3), (uint8_t)((raw & 7) << 5), 0, 0, 0, 0, 0, 0 };
    feed(0x1DB, d);
}

static void tx_55b(float soc)
{
    int raw = (int)lroundf(soc * 10.0f);
    uint8_t d[8] = { (uint8_t)(raw >> 2), (uint8_t)((raw & 3) << 6), 0, 0, 0, 0, 0, 0 };
    feed(0x55B, d);
}

static void tx_55a(float inv_c, float motor_c)
{
    uint8_t d[8] = { 0, (uint8_t)lroundf(motor_c * 9 / 5 + 32), (uint8_t)lroundf(inv_c * 9 / 5 + 32), 0, 0, 0, 0, 0 };
    feed(0x55A, d);
}

static void tx_390(uint8_t obc, bool plug)
{
    uint8_t d[8] = { 0, 0, 0, (uint8_t)(obc << 3), 0, plug ? 0x08 : 0x00, 0, 0 };
    feed(0x390, d);
}

static void tx_31a(int dir, uint8_t opmode, float aux)
{
    uint16_t raw = (uint16_t)lroundf(aux * 10.0f);
    uint8_t d[8] = { (uint8_t)(int8_t)dir, opmode, (uint8_t)raw, (uint8_t)(raw >> 8), 0, 0, 0, 0 };
    feed(0x31A, d);
}

// ── scenarios ──────────────────────────────────────────────────────────────

const char *const sim_scenarios[] = {
    "drive", "regen", "charging", "lowbatt", "fault", "nodata", "metric", "service", "test", NULL,
};

bool sim_model_init(const char *scenario, uint32_t now_ms)
{
    memset(&g_snap, 0, sizeof(g_snap));
    dash_state_init(&g_snap.vehicle);
    g_snap.now_ms = now_ms;
    g_snap.odo_m = 150234.0 * DASH_METERS_PER_MILE;
    g_snap.settings.use_mph = true;
    g_snap.settings.use_odo_miles = true;
    g_snap.can_state = DASH_CAN_OK;
    g_animated = false;

    if (strcmp(scenario, "nodata") == 0) {
        return true;
    }
    if (strcmp(scenario, "test") == 0) {
        g_animated = true;
        g_snap.can_state = DASH_CAN_TEST_MODE;
        dash_test_data(&g_snap.vehicle, now_ms);
        return true;
    }

    // Baseline: cruising forward.
    tx_1da(352.5f, 5200, false);
    tx_1db(-118);
    tx_55b(68.4f);
    tx_55a(48, 61);
    tx_390(0, false);
    tx_31a(1, OPMODE_RUN, 13.8f);

    if (strcmp(scenario, "drive") == 0 || strcmp(scenario, "service") == 0) {
        return true;
    }
    if (strcmp(scenario, "metric") == 0) {
        g_snap.settings = (dash_settings_t){ .use_mph = false, .use_fahrenheit = false, .use_odo_miles = false };
        return true;
    }
    if (strcmp(scenario, "regen") == 0) {
        tx_1da(371.0f, 3100, false);
        tx_1db(64);
        return true;
    }
    if (strcmp(scenario, "charging") == 0) {
        tx_1da(364.0f, 0, false);
        tx_1db(18);  // ~6.6 kW into the pack
        tx_55b(41.0f);
        tx_390(2, true);
        tx_31a(0, OPMODE_CHARGING, 14.1f);
        return true;
    }
    if (strcmp(scenario, "lowbatt") == 0) {
        tx_55b(7.5f);
        tx_1da(318.0f, 2600, false);
        tx_1db(-62);
        tx_31a(1, OPMODE_RUN, 12.2f);
        return true;
    }
    if (strcmp(scenario, "fault") == 0) {
        tx_1da(349.0f, 1800, true);
        tx_55a(83, 88);
        tx_31a(-1, OPMODE_RUN, 11.3f);
        return true;
    }
    return false;
}

void sim_model_tick(uint32_t now_ms)
{
    g_snap.now_ms = now_ms;
    if (g_animated) {
        dash_test_data(&g_snap.vehicle, now_ms);
    }
}

void dash_model_snapshot(dash_snapshot_t *out) { *out = g_snap; }

void dash_model_set_settings(const dash_settings_t *cfg) { g_snap.settings = *cfg; }
