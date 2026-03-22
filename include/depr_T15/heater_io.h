#pragma once

#include <stdbool.h>
#include <stdint.h>

namespace t15_heater
{
void pwm_stop();
bool pwm_start(uint8_t dutyPercent);
bool is_running();
} // namespace t15_heater
