#pragma once
#include <stdint.h>

namespace t13::hotspot {
// Hotspot NTC divider configuration
// Hardware: 5V -> Rfixed -> ADC tap -> NTC -> GND
static constexpr int32_t NTC_VREF_MV = 5000;
static constexpr int32_t NTC_R_FIXED_OHM = 100000; // 100k pull-up
static constexpr bool    NTC_TO_GND = true;
} // namespace t13::hotspot
