#pragma once
// iOVEO 036HS05201 – 10k NTC (values in kΩ converted to Ω)
#include "ntc/ntc_types.h"

namespace ntc {
static constexpr NtcTableMode kNtcTableMode = NtcTableMode::Resistance_Ohm;

// HotSpot NTC
static constexpr NtcPoint kNtc100k_Hotspot_Table[] = {
    {0 * 10, 327240},
    {10 * 10, 199990},
    {20 * 10, 125245},
    {25 * 10, 100000}, // important anchor point (R25)
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

// static constexpr NtcPoint kNtc100k_Hotspot_Table[] = {
//     {0 * 10, 32650},
//     {10 * 10, 19900},
//     {20 * 10, 12490},
//     {25 * 10, 10000},
//     {30 * 10, 8060},
//     {40 * 10, 5320},
//     {50 * 10, 3620},
//     {60 * 10, 2490},
//     {70 * 10, 1750},
//     {80 * 10, 1260},
//     {90 * 10, 920},
//     {100 * 10, 680},
//     {110 * 10, 510},
//     {120 * 10, 390},
//     {130 * 10, 300},
//     {140 * 10, 230},
//     {150 * 10, 180},
// };
static constexpr uint16_t kNtc100k_Hotspot_TableCount = sizeof(kNtc100k_Hotspot_Table) / sizeof(kNtc100k_Hotspot_Table[0]);

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