#pragma once
#include <stdint.h>
namespace t13 {
static constexpr uint8_t ADS_CH_HOTSPOT = 0;
static constexpr uint8_t ADS_CH_CHAMBER = 1;
static constexpr float ADS_LSB_MV = 0.1875f;
static constexpr int16_t TEMP_INVALID_DC = (int16_t)-32768;
}