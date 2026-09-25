#pragma once

#include "esp_err.h"

// Load settings/odometer from NVS, start CAN (or test mode) and the odometer task.
esp_err_t dash_model_start(void);
