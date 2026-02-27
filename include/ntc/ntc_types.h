#pragma once
#include <stdint.h>
namespace ntc {
struct NtcPoint {
    int16_t temp_dC;
    int32_t value;
};
enum class NtcTableMode : uint8_t { Resistance_Ohm = 0,
                                    Voltage_mV = 1,
                                    Ratio_Permille = 2 };
} // namespace ntc