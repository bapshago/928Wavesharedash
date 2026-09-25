// Nissan Leaf / ZombieVerter CAN decoding, ported from can_rx_task() in the
// Angry Pixie web dash. Pure function: no locking, no driver calls.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dash_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Decode one standard-ID CAN frame into *s. data must point at 8 readable
// bytes (zero-padded past dlc). Returns true if the ID was recognised and the
// frame was applied. Frames shorter than 6 bytes are ignored, as before.
bool leaf_can_decode(uint32_t id, const uint8_t data[8], uint8_t dlc, dash_state_t *s);

#ifdef __cplusplus
}
#endif
