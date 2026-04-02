#include "sensors/sensor_ntc.h"

#include <Arduino.h>
#include <Wire.h>

#include "depr_T15/t15_log_tags.h"
#include "log_core.h"
#include "pins_client.h"

#include "ntc/ntc.h"
#include "ntc/ntc_convert.h"
#include "ntc/ntc_table_hotspot.h"
#include "ntc/ntc_table_10k_ioveo_036HS05201.h"

#include "Adafruit_ADS1X15.h"

namespace t15_sensor {
static constexpr uint8_t I2C_SDA = 22;
static constexpr uint8_t I2C_SCL = 23;
static constexpr uint8_t I2C_ADR = 0x48;

static constexpr int32_t VREF_MV = 5000;
static constexpr int32_t RFIXED_HOT_OHM = 100000;
static constexpr int32_t RFIXED_CHA_OHM = 10000;
static constexpr bool NTC_TO_GND = true;

static constexpr int32_t HOT_MV_MIN_VALID = 50;
static constexpr int32_t HOT_MV_MAX_VALID = (VREF_MV - 50);

static Adafruit_ADS1115 g_ads;
static ChamberSample g_sample;
static ChamberSample g_sample_next;

static void i2cScan() {
    T15_INFO(t15_log::BASE, "[I2C] scan...\n");
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        const uint8_t err = Wire.endTransmission();

        if (err == 0) {
            T15_INFO(t15_log::BASE, "[I2C] found 0x%02X\n", addr);
            count++;
        }

        delay(2);
    }

    if (count == 0) {
        T15_WARN(t15_log::BASE, "[I2C] no devices found\n");
    }
}

bool is_door_open() {
    return (digitalRead(OVEN_DOOR_SENSOR) != 0);
}

void init_door() {
    pinMode(OVEN_DOOR_SENSOR, INPUT_PULLUP);

    const bool door_open = is_door_open();
    T15_INFO(t15_log::BASE,
             "[IO] DOOR init done: GPIO=%d INPUT_PULLUP level=%d (%s)\n",
             OVEN_DOOR_SENSOR,
             door_open ? 1 : 0,
             door_open ? "OPEN" : "CLOSED");
}

void init_i2c_and_ads() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    T15_INFO(t15_log::BASE,
             "[I2C] init: SDA=%u SCL=%u clock=100kHz\n",
             (unsigned)I2C_SDA,
             (unsigned)I2C_SCL);

    i2cScan();

    if (!g_ads.begin(I2C_ADR, &Wire)) {
        T15_ERROR(t15_log::BASE,
                  "[I2C] ADS1115 not found at 0x%02X. Check wiring/address\n",
                  (unsigned)I2C_ADR);
        T15_ERROR(t15_log::BASE,
                  "[I2C] Tip: ADS1115 addresses are usually 0x48,0x49,0x4A,0x4B.\n");

        g_sample.adsOk = false;
        g_sample_next.adsOk = false;

        while (true) {
            delay(1000);
        }
    } else {
        g_sample.adsOk = true;
        g_sample_next.adsOk = true;
        g_ads.setGain(GAIN_TWOTHIRDS);

        T15_INFO(t15_log::BASE,
                 "[I2C] ADS1115 found, Gain=%d (6.144V)\n",
                 (int)GAIN_TWOTHIRDS);
    }
}

static void sample_hotspot_temperature() {
    g_sample_next.rawHotspot = g_ads.readADC_SingleEnded(0); // A0 = Hotspot
    g_sample_next.hot_mV = ntc::ads_raw_to_mV(g_sample_next.rawHotspot);

    const bool railInvalid =
        (g_sample_next.hot_mV <= HOT_MV_MIN_VALID) ||
        (g_sample_next.hot_mV >= HOT_MV_MAX_VALID);

    g_sample_next.hot_ohm = ntc::voltage_to_resistance_ohm(
        g_sample_next.hot_mV,
        VREF_MV,
        RFIXED_HOT_OHM,
        NTC_TO_GND);

    g_sample_next.hot_dC = ntc::calc_temp_from_ads_raw_dC(
        g_sample_next.rawHotspot,
        ntc::kNtc100k_Hotspot_Table,
        ntc::kNtc100k_Hotspot_TableCount,
        ntc::NtcTableMode::Resistance_Ohm,
        VREF_MV,
        RFIXED_HOT_OHM,
        NTC_TO_GND);

    const bool convInvalid =
        (g_sample_next.hot_ohm < 0) ||
        (g_sample_next.hot_dC == ntc::TEMP_INVALID_DC);

    g_sample_next.hotValid = !(railInvalid || convInvalid);
    g_sample_next.tempHotspotC =
        g_sample_next.hotValid ? (g_sample_next.hot_dC / 10.0f) : NAN;
}

void sample_chamber_temperature() {
    if (!g_sample.adsOk) {
        return;
    }

    g_sample_next = g_sample;

    sample_hotspot_temperature();

    g_sample_next.rawChamber = g_ads.readADC_SingleEnded(1); // A1 = Chamber
    g_sample_next.cha_mV = ntc::ads_raw_to_mV(g_sample_next.rawChamber);
    g_sample_next.cha_ohm = ntc::voltage_to_resistance_ohm(
        g_sample_next.cha_mV,
        VREF_MV,
        RFIXED_CHA_OHM,
        NTC_TO_GND);

    g_sample_next.cha_dC = ntc::calc_temp_from_ads_raw_dC(
        g_sample_next.rawChamber,
        ntc::kNtc10k_Chamber_Table,
        ntc::kNtc10k_Chamber_TableCount,
        ntc::NtcTableMode::Resistance_Ohm,
        VREF_MV,
        RFIXED_CHA_OHM,
        NTC_TO_GND);

    g_sample_next.tempChamberC =
        (g_sample_next.cha_dC == ntc::TEMP_INVALID_DC)
            ? NAN
            : (g_sample_next.cha_dC / 10.0f);

    g_sample_next.adsOk = g_sample.adsOk;
    g_sample = g_sample_next;
}

const ChamberSample &get_sample() {
    return g_sample;
}

} // namespace t15_sensor
