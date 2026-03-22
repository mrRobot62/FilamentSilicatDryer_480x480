#pragma once

#include <stdint.h>

namespace heater_io
{
    static constexpr uint8_t kDefaultDutyPercent = 50;

    void init();
    void set_enabled(bool enabled);
}
