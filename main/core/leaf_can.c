#include "leaf_can.h"

#include <math.h>
#include <string.h>

void dash_state_init(dash_state_t *s)
{
    memset(s, 0, sizeof(*s));
    // Unknown until the first frame: shown as "--", and no false low-battery
    // warning at key-on before 0x55B arrives.
    s->voltage_v = NAN;
    s->current_a = NAN;
    s->soc_pct = NAN;
    s->inv_temp_c = NAN;
    s->motor_temp_c = NAN;
    s->aux_12v = NAN;
}

static inline float fahrenheit_to_celsius(uint8_t f)
{
    return ((float)f - 32.0f) * (5.0f / 9.0f);
}

bool leaf_can_decode(uint32_t id, const uint8_t d[8], uint8_t dlc, dash_state_t *s)
{
    if (dlc < 6) {
        return false;
    }

    switch (id) {
    case 0x1DA: {  // Inverter: DC bus voltage, motor speed, error flags
        int32_t raw_voltage = ((int32_t)d[0] << 2) | (d[1] >> 6);
        s->voltage_v = raw_voltage * 0.5f;

        int16_t parsed_speed = (int16_t)((d[4] << 8) | d[5]);
        s->motor_rpm = (parsed_speed == 0x7FFF) ? 0.0f : parsed_speed * 0.5f;

        s->inverter_error = (d[6] & 0xB0) != 0x00;
        return true;
    }

    case 0x55A:  // Inverter + motor temperature, reported in °F
        s->inv_temp_c = fahrenheit_to_celsius(d[2]);
        s->motor_temp_c = fahrenheit_to_celsius(d[1]);
        return true;

    case 0x390:  // OBC / charge plug status
        s->obc_volt_stat = (d[3] >> 3) & 0x03;
        s->plug_stat = d[5] & 0x0F;
        s->plug_inserted = (s->plug_stat == 0x08);
        return true;

    case 0x55B: {  // State of charge, 0.1 %/bit
        uint16_t raw = (uint16_t)((d[0] << 2) | (d[1] >> 6));
        s->soc_pct = raw * 0.1f;
        return true;
    }

    case 0x1DB: {  // Battery current: 11-bit two's complement, 1 A/bit as in the web dash
        int32_t raw = ((int32_t)d[0] << 3) | (d[1] >> 5);
        if (raw > 1023) {
            raw -= 2048;
        }
        s->current_a = (float)raw;
        return true;
    }

    case 0x292:  // Stock Leaf CAR-CAN: LeadAcidBatteryVoltage in byte 3, 0.1 V/bit
        s->aux_12v = d[3] * 0.1f;
        return true;

    case 0x31A: {  // ZombieVerter custom TX map (see docs/ZombieVerter_CAN_Mappings.md)
        switch (d[0]) {
        case 0x01: s->drive_dir = 1; break;
        case 0xFF: s->drive_dir = -1; break;
        default:   s->drive_dir = 0; break;
        }
        s->opmode = (d[1] <= OPMODE_CHARGING) ? d[1] : OPMODE_OFF;

        // uaux mapped at start bit 16, length 16, gain 10: little-endian, 0.1 V/bit.
        // Zero means the mapping is missing, so keep whatever we had.
        uint16_t raw12 = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
        if (raw12 != 0) {
            s->aux_12v = raw12 * 0.1f;
        }
        return true;
    }

    default:
        return false;
    }
}
