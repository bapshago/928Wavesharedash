// The dash UI: a driver screen (driverpage.h in the web dash) and a service
// screen (diagnosticpage.h), side by side in a tileview — swipe to switch.
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Build the UI on the given screen and start its refresh timers. Must be
// called with the LVGL lock held. Data comes from dash_model_snapshot().
void dash_ui_create(lv_obj_t *screen);

// Show the driver (0) or service (1) page.
void dash_ui_show_page(int page);

#ifdef __cplusplus
}
#endif
