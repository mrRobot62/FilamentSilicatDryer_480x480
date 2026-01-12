#pragma once

#include "oven.h" // for OVEN_CONNECTOR
#include <stdint.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Debug helper: decode outputs bitmask into readable text
//
// Example output:
//   "0x0023 FAN12V FAN230 LAMP"
//
// - No heap allocation
// - Thread-unsafe (static buffer) → intended for logging only
// - Header-only, usable everywhere
// -----------------------------------------------------------------------------
inline const char *oven_outputs_mask_to_str(uint16_t mask) {
    static char buf[96];
    char *p = buf;
    size_t left = sizeof(buf);

    // Always print raw mask first
    int n = snprintf(p, left, "0x%04X", mask);
    if (n < 0 || (size_t)n >= left) {
        return buf;
    }
    p += n;
    left -= n;

    auto add = [&](const char *name) {
        if (left <= 1) {
            return;
        }
        int k = snprintf(p, left, " %s", name);
        if (k > 0 && (size_t)k < left) {
            p += k;
            left -= k;
        }
    };

    if (mask & (uint16_t)OVEN_CONNECTOR::FAN12V) {
        add("FAN12V");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::FAN230V) {
        add("FAN230");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::FAN230V_SLOW) {
        add("FAN230_SLOW");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::SILICAT_MOTOR) {
        add("MOTOR");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::HEATER) {
        add("HEATER");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::LAMP) {
        add("LAMP");
    }
    if (mask & (uint16_t)OVEN_CONNECTOR::DOOR_ACTIVE) {
        add("DOOR");
    }

    return buf;
}

// END OF FILE
