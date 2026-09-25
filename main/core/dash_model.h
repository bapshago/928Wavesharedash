// What the UI needs from the platform: a consistent snapshot of the vehicle
// state plus the persisted settings. Implemented by main/dash_model.c on the
// ESP32-P4 (TWAI + NVS) and by sim/sim_model.c on the desktop simulator.
// This replaces the /api/data JSON endpoint of the web dash.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dash_calc.h"
#include "dash_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DASH_CAN_DOWN = 0,   // driver failed to start
    DASH_CAN_OK,
    DASH_CAN_WARNING,
    DASH_CAN_PASSIVE,
    DASH_CAN_BUS_OFF,
    DASH_CAN_TEST_MODE,  // no bus: animated fake data
} dash_can_state_t;

typedef struct {
    dash_state_t     vehicle;
    dash_settings_t  settings;
    double           odo_m;          // odometer, metres
    dash_can_state_t can_state;
    uint32_t         now_ms;         // for "last frame N ms ago"
} dash_snapshot_t;

void dash_model_snapshot(dash_snapshot_t *out);

// Change and persist the unit settings (the web dash's /api/set* endpoints).
void dash_model_set_settings(const dash_settings_t *cfg);

const char *dash_can_state_text(dash_can_state_t st);

#ifdef __cplusplus
}
#endif
