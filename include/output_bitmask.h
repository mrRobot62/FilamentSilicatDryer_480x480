#pragma once
#include <Arduino.h>

// --------------------------------------
// Bitmask for Status
//
// used by oven & Client
// --------------------------------------

enum OUTPUT_BIT_MASK_8BIT : uint8_t {
    BIT_FAN12V = 0,
    BIT_FAN230V = 1,
    BIT_LAMP = 2,
    BIT_SILICA_MOTOR = 3,
    BIT_FAN230V_SLOW = 4,
    BIT_DOOR = 5,
    BIT_HEATER = 6,
    BIT_RESERVE = 7
};
