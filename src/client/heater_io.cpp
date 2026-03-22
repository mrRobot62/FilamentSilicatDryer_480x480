#include "client/heater_io.h"

// NOTE:
// This is a direct extraction from T15 heater_io.cpp.
// Behavior must stay identical (4kHz, ~50% duty, never 100%).

namespace heater_io
{
    void init()
    {
        // TODO: move LEDC / PWM init from T15
    }

    void set_enabled(bool enabled)
    {
        if (enabled)
        {
            // start PWM (approx 50% duty)
        }
        else
        {
            // stop PWM
        }
    }
}
