#pragma once

#include "host_parameters.h"

#include <stdint.h>

bool host_curve_profile_index_valid(uint8_t profileIndex);

HostHeaterProfileParameters host_curve_default_heater_profile(uint8_t profileIndex);
HostPulseCurveParameters host_curve_default_pulse_curve(uint8_t profileIndex);

bool host_curve_validate_heater_profile(const HostHeaterProfileParameters &profile);
bool host_curve_validate_pulse_curve(const HostPulseCurveParameters &pulse);
