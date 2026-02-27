#include "ntc/ntc_convert.h"

namespace ntc {

static inline bool is_monotonic_increasing(const NtcPoint *p, uint16_t n) {
    return (n >= 2) ? (p[n - 1].value > p[0].value) : true;
}

int16_t ntc_table_lookup_temp_dC(const NtcPoint *points, uint16_t count, int32_t inputValue) {
    if (!points || count < 2) {
        return TEMP_INVALID_DC;
    }

    const bool inc = is_monotonic_increasing(points, count);

    const int32_t v0 = points[0].value;
    const int32_t vN = points[count - 1].value;
    const int32_t vmin = inc ? v0 : vN;
    const int32_t vmax = inc ? vN : v0;

    if (inputValue < vmin || inputValue > vmax) {
        return TEMP_INVALID_DC;
    }

    for (uint16_t i = 0; i < count - 1; ++i) {
        const int32_t a = points[i].value;
        const int32_t b = points[i + 1].value;
        const bool inSeg = inc ? (inputValue >= a && inputValue <= b) : (inputValue <= a && inputValue >= b);
        if (!inSeg) {
            continue;
        }

        const int16_t tA = points[i].temp_dC;
        const int16_t tB = points[i + 1].temp_dC;

        if (a == b) {
            return tA;
        }

        const float frac = (float)(inputValue - a) / (float)(b - a);
        const float t = (float)tA + frac * (float)(tB - tA);
        return (int16_t)(t >= 0 ? (t + 0.5f) : (t - 0.5f));
    }

    return TEMP_INVALID_DC;
}

int32_t voltage_to_resistance_ohm(int32_t vntc_mV, int32_t vref_mV, int32_t r_fixed_ohm, bool ntc_to_gnd) {
    if (vref_mV <= 0 || r_fixed_ohm <= 0) {
        return -1;
    }
    if (vntc_mV <= 0 || vntc_mV >= vref_mV) {
        return -1;
    }

    if (ntc_to_gnd) {
        return (int32_t)((int64_t)r_fixed_ohm * (int64_t)vntc_mV / (int64_t)(vref_mV - vntc_mV));
    } else {
        return (int32_t)((int64_t)r_fixed_ohm * (int64_t)(vref_mV - vntc_mV) / (int64_t)vntc_mV);
    }
}

int16_t calc_temp_from_ads_raw_dC(
    int16_t rawAdc,
    const NtcPoint *tablePoints,
    uint16_t tableCount,
    NtcTableMode mode,
    int32_t vref_mV,
    int32_t r_fixed_ohm,
    bool ntc_to_gnd) {

    if (!tablePoints || tableCount < 2) {
        return TEMP_INVALID_DC;
    }

    switch (mode) {
    case NtcTableMode::Resistance_Ohm: {
        const int32_t vntc_mV = ads_raw_to_mV(rawAdc);
        const int32_t r_ohm = voltage_to_resistance_ohm(vntc_mV, vref_mV, r_fixed_ohm, ntc_to_gnd);
        if (r_ohm <= 0) {
            return TEMP_INVALID_DC;
        }
        return ntc_table_lookup_temp_dC(tablePoints, tableCount, r_ohm);
    }
    case NtcTableMode::Voltage_mV: {
        const int32_t vntc_mV = ads_raw_to_mV(rawAdc);
        return ntc_table_lookup_temp_dC(tablePoints, tableCount, vntc_mV);
    }
    case NtcTableMode::Ratio_Permille: {
        if (vref_mV <= 0) {
            return TEMP_INVALID_DC;
        }
        const int32_t vntc_mV = ads_raw_to_mV(rawAdc);
        const int32_t ratio = (int32_t)((int64_t)vntc_mV * 1000LL / (int64_t)vref_mV);
        return ntc_table_lookup_temp_dC(tablePoints, tableCount, ratio);
    }
    default:
        return TEMP_INVALID_DC;
    }
}

} // namespace ntc
