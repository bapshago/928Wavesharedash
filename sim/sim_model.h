#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dash_model.h"

// NULL-terminated list of scenario names.
extern const char *const sim_scenarios[];

// Load a named scenario. Returns false if the name is unknown.
bool sim_model_init(const char *scenario, uint32_t now_ms);

// Advance simulated time (animates the "test" scenario).
void sim_model_tick(uint32_t now_ms);
