#pragma once

#include "ntc/ntc_types.h"

namespace ntc {

// Approximated table for the original built-in hotspot NTC near the heater.
// The real vendor curve is currently unknown and should be treated as empirical.
static constexpr NtcTableMode kNtcHotspotTableMode = NtcTableMode::Resistance_Ohm;

static constexpr NtcPoint kNtc100k_Hotspot_Table[] = {
    {0 * 10, 327240},
    {10 * 10, 199990},
    {20 * 10, 125245},
    {25 * 10, 100000},
    {30 * 10, 81000},
    {40 * 10, 53500},
    {50 * 10, 35899},
    {60 * 10, 25000},
    {70 * 10, 17550},
    {80 * 10, 12540},
    {90 * 10, 9100},
    {100 * 10, 6710},
    {110 * 10, 5070},
    {120 * 10, 3850},
    {130 * 10, 2940},
    {140 * 10, 2271},
    {150 * 10, 1770},
    {160 * 10, 1414},
    {170 * 10, 1122},
    {180 * 10, 896},
    {190 * 10, 719},
    {200 * 10, 582},
    {210 * 10, 483},
    {220 * 10, 396},
    {230 * 10, 328},
    {240 * 10, 274},
    {250 * 10, 230},
    {260 * 10, 195},
    {270 * 10, 166},
    {280 * 10, 142},
    {290 * 10, 122},
    {300 * 10, 106},
};

static constexpr uint16_t kNtc100k_Hotspot_TableCount =
    sizeof(kNtc100k_Hotspot_Table) / sizeof(kNtc100k_Hotspot_Table[0]);

} // namespace ntc
