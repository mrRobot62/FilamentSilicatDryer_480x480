#pragma once

#include <stdbool.h>
#include <stdint.h>

namespace heater_io {

static constexpr uint32_t kPwmFreqHz = 4000;
static constexpr uint8_t kPwmResBits = 10;
static constexpr uint8_t kDefaultDutyPercent = 50;

void init_off();
bool set_enabled(bool enabled, uint8_t dutyPercent = kDefaultDutyPercent);
bool is_running();

} // namespace heater_io
