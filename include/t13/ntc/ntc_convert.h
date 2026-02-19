#pragma once
#include <stdint.h>
#include "t13/sensors/ads1115_config.h"
#include "t13/ntc/ntc_types.h"
namespace t13 {
int16_t ntc_table_lookup_temp_dC(const NtcPoint* points, uint16_t count, int32_t inputValue);
inline int32_t ads_raw_to_mV(int16_t raw){ return (int32_t)((float)raw * ADS_LSB_MV); }
int32_t voltage_to_resistance_ohm(int32_t vntc_mV,int32_t vref_mV,int32_t r_fixed_ohm,bool ntc_to_gnd);
int16_t calc_temp_from_ads_raw_dC(int16_t rawAdc,const NtcPoint* tablePoints,uint16_t tableCount,NtcTableMode mode,int32_t vref_mV,int32_t r_fixed_ohm,bool ntc_to_gnd);
}