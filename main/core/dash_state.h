// Vehicle state shared between the CAN receiver and the UI.
//
// This is the on-screen equivalent of the volatile g_* globals in the
// Angry Pixie web dash (leaf_dash_v9.ino). It is plain C with no ESP-IDF
// dependencies so the decoder and calculations can be unit-tested and the UI
// rendered on a desktop (see host_test/ and sim/).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Op modes reported by ZombieVerter in 0x31A byte 1.
typedef enum {
    OPMODE_OFF = 0,
    OPMODE_RUN = 1,
    OPMODE_PRECHARGE = 2,
    OPMODE_PRECHARGE_FAIL = 3,
    OPMODE_CHARGING = 4,
} dash_opmode_t;

typedef struct {
    // Values that haven't been received yet are NAN.
    // 0x1DA: inverter
    float   voltage_v;       // DC bus voltage
    float   motor_rpm;
    bool    inverter_error;
    // 0x55A: temperatures (NAN until the first frame arrives)
    float   inv_temp_c;
    float   motor_temp_c;
    // 0x390: on-board charger / plug
    uint8_t obc_volt_stat;
    uint8_t plug_stat;
    bool    plug_inserted;   // PPStat in the web dash
    // 0x55B: state of charge (%)
    float   soc_pct;
    // 0x1DB: battery current (A). Negative = throttle, positive = regen,
    // matching the web dash's current gauge.
    float   current_a;
    // 0x31A (ZombieVerter custom TX mapping) / 0x292 (stock Leaf CAR-CAN)
    int8_t  drive_dir;       // -1 reverse, 0 neutral, 1 forward
    uint8_t opmode;          // dash_opmode_t
    float   aux_12v;         // NAN until known
    uint8_t raw_31a[8];      // last raw 0x31A frame, for diagnostics
    uint8_t raw_31a_dlc;     // 0 = no 0x31A frame received yet

    // Bookkeeping for the service screen.
    uint32_t frames_rx;      // total frames seen
    uint32_t frames_decoded; // frames with an ID we understand
    uint32_t last_rx_ms;     // timestamp of the last frame (0 = never)
} dash_state_t;

// Reset to the power-on state (unknown values = NAN / zero).
void dash_state_init(dash_state_t *s);

#ifdef __cplusplus
}
#endif
