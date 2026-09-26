// The dash UI: a driver screen (driverpage.h in the web dash) and a service
// screen (diagnosticpage.h), side by side in a tileview — swipe to switch.
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Which of the two dash units this is. Both run the same firmware; the side
// (set in menuconfig) decides which gauges the driver page shows.
typedef enum {
    DASH_SIDE_LEFT = 0,   // speed, drive direction, odometer, state of charge
    DASH_SIDE_RIGHT = 1,  // current, voltage, 12V, temperatures, op mode, plug/charger
} dash_side_t;

// Build the UI on the given screen and start its refresh timers. Must be
// called with the LVGL lock held. Data comes from dash_model_snapshot().
void dash_ui_create(lv_obj_t *screen, dash_side_t side);

// Show the driver (0) or service (1) page.
void dash_ui_show_page(int page);

#ifdef __cplusplus
}
#endif
