#pragma once
#include <stdint.h>

namespace ntc::chamber {
// Chamber NTC divider configuration
// Hardware: 5V -> Rfixed -> ADC tap -> NTC -> GND
static constexpr int32_t NTC_VREF_MV = 5000;
static constexpr int32_t NTC_R_FIXED_OHM = 10000; // 10k pull-up
static constexpr bool    NTC_TO_GND = true;
} // namespace ntc::chamber
