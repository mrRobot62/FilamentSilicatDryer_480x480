#include "client/sensor_ntc.h"

// NOTE:
// This is a direct extraction from T15 sensor logic.
// Keep behavior identical for now (no functional changes).
// Replace internals later if needed.

namespace sensor_ntc
{
    void init()
    {
        // TODO: move init code from T15 sensor_ntc.cpp
    }

    Sample read()
    {
        Sample s{};

        // TODO: move actual T15 logic here:
        // - read ADC
        // - convert using ntc tables
        // - fill chamberC / hotspotC
        // - set valid flags

        s.chamberC = 0.0f;
        s.hotspotC = 0.0f;
        s.chamberValid = false;
        s.hotspotValid = false;

        return s;
    }
}
