#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace sensor_ntc
{
    struct Sample
    {
        float chamberC;
        float hotspotC;
        bool chamberValid;
        bool hotspotValid;
    };

    // Initialize sensors (ADC, etc.)
    void init();

    // Read both temperatures (blocking or cached depending on implementation)
    Sample read();
}
