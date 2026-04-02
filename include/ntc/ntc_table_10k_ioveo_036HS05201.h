#pragma once
// iOVEO 036HS05201 – chamber 10k NTC (values in kOhm converted to Ohm)
#include "ntc/ntc_types.h"

namespace ntc {
static constexpr NtcTableMode kNtcChamberTableMode = NtcTableMode::Resistance_Ohm;

// Chamber NTC
static constexpr NtcPoint kNtc10k_Chamber_Table[] = {
    {0 * 10, 32650},
    {10 * 10, 19900},
    {20 * 10, 12490},
    {25 * 10, 10000},
    {30 * 10, 8060},
    {40 * 10, 5320},
    {50 * 10, 3620},
    {60 * 10, 2490},
    {70 * 10, 1750},
    {80 * 10, 1260},
    {90 * 10, 920},
    {100 * 10, 680},
    {110 * 10, 510},
    {120 * 10, 390},
    {130 * 10, 300},
    {140 * 10, 230},
    {150 * 10, 180},
};
static constexpr uint16_t kNtc10k_Chamber_TableCount = sizeof(kNtc10k_Chamber_Table) / sizeof(kNtc10k_Chamber_Table[0]);

} // namespace ntc
