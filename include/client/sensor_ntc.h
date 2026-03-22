#pragma once

#include <stdint.h>

namespace sensor_ntc {

struct Sample {
    bool adsOk = false;

    int16_t rawHotspot = 0;
    int32_t hot_mV = 0;
    int32_t hot_ohm = -1;
    bool hotValid = false;
    int16_t hot_dC = -32768;
    float tempHotspotC = 0.0f / 0.0f; // NAN

    int16_t rawChamber = 0;
    int32_t cha_mV = 0;
    int32_t cha_ohm = 0;
    int16_t cha_dC = -32768;
    float tempChamberC = 0.0f / 0.0f; // NAN
};

void init_door();
void init_i2c_and_ads();
bool is_door_open();

void sample_temperatures();
const Sample &get_sample();

} // namespace sensor_ntc
