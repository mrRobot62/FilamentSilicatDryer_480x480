#include <Arduino.h>
#include <Wire.h>

#include "log_core.h"
#include "udp/fsd_udp.h"

#include "pins_client.h"

#include "ntc/ntc.h"
#include "ntc/ntc_convert.h"
#include "ntc/ntc_table_10k_ioveo_036HS05201.h"

#include "T15/t15_log_tags.h"

#include "Adafruit_ADS1X15.h"

// --------------------------------------------------------------------
// T15 Step 3A.1
// Chamber-only test mode for T15-2.2 (heater OFF runout / tau estimation)
//
// Facts for this step:
// - Only TChamber is available / valid
// - Chamber must use the manufacturer NTC table
// - Hotspot is not available and therefore not used
// - Door open => heater forced OFF
// - Logs are classified for UDP filtering
// --------------------------------------------------------------------

// I2C / ADS
static constexpr uint8_t I2C_SDA = 22;
static constexpr uint8_t I2C_SCL = 23;
static constexpr uint8_t I2C_ADR = 0x48;

// Chamber divider domain
static constexpr int32_t VREF_MV = 5000;
static constexpr int32_t RFIXED_CHA_OHM = 10000;
static constexpr bool NTC_TO_GND = true;

// Heater test behavior
static constexpr uint32_t HEATER_PWM_FREQ_HZ = 4000;
static constexpr uint8_t HEATER_PWM_RES_BITS = 10;
static constexpr uint8_t HEATER_TEST_DUTY_PERCENT = 100;
static constexpr int HEATER_SAFE_LEVEL = LOW;

// Sampling / logging
static constexpr uint32_t SAMPLE_INTERVAL_MS = 250;
static constexpr uint32_t LOG_INTERVAL_MS = 1000;

// Test 2.2 state
enum class Test22State : uint8_t {
    IDLE = 0,
    HEATING_TO_TARGET,
    RUNOUT_AFTER_OFF,
    PEAK_REACHED
};

static Adafruit_ADS1115 ads;
static bool g_adsOk = false;

// Chamber measurement only
static int16_t g_rawChamber = 0;
static int32_t g_cha_mV = 0;
static int32_t g_cha_ohm = 0;
static int16_t g_cha_dC = ntc::TEMP_INVALID_DC;
static float g_tempChamberC = NAN;

static uint32_t g_lastSampleMs = 0;
static uint32_t g_lastLogMs = 0;

// Heater / test state
static bool g_heaterPwmRunning = false;
static Test22State g_test22State = Test22State::IDLE;
static bool g_test22Started = false;
static uint32_t g_heaterOffMs = 0;
static int16_t g_chamberAtOff_dC = ntc::TEMP_INVALID_DC;
static int16_t g_peakChamber_dC = ntc::TEMP_INVALID_DC;
static uint32_t g_peakAtMs = 0;
static uint8_t g_coolingConfirmCount = 0;

// --------------------------------------------------------------------
// Helper
// --------------------------------------------------------------------
static bool isDoorOpen() {
    return (digitalRead(OVEN_DOOR_SENSOR) != 0);
}

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

static void initDoor() {
    pinMode(OVEN_DOOR_SENSOR, INPUT_PULLUP);

    const bool door_open = isDoorOpen();
    T15_INFO(t15_log::BASE,
             "[IO] DOOR init done: GPIO=%d INPUT_PULLUP level=%d (%s)\n",
             OVEN_DOOR_SENSOR,
             door_open ? 1 : 0,
             door_open ? "OPEN" : "CLOSED");
}

static void initI2CAndADS() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    T15_INFO(t15_log::BASE,
             "[I2C] init: SDA=%u SCL=%u clock=100kHz\n",
             (unsigned)I2C_SDA,
             (unsigned)I2C_SCL);

    i2cScan();

    if (!ads.begin(I2C_ADR, &Wire)) {
        T15_ERROR(t15_log::BASE,
                  "[I2C] ADS1115 not found at 0x%02X. Check wiring/address\n",
                  (unsigned)I2C_ADR);
        T15_ERROR(t15_log::BASE,
                  "[I2C] Tip: ADS1115 addresses are usually 0x48,0x49,0x4A,0x4B.\n");

        g_adsOk = false;
        while (true) { delay(1000); }
    } else {
        g_adsOk = true;
        ads.setGain(GAIN_TWOTHIRDS);

        T15_INFO(t15_log::BASE,
                 "[I2C] ADS1115 found, Gain=%d (6.144V)\n",
                 (int)GAIN_TWOTHIRDS);
    }
}

// --------------------------------------------------------------------
// Heater PWM
// --------------------------------------------------------------------
static inline uint32_t heaterDutyFromPercent(uint8_t pct) {
    const uint32_t maxDuty = (1u << HEATER_PWM_RES_BITS) - 1u;
    return (maxDuty * (uint32_t)pct) / 100u;
}

static void heaterPwmStop() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (g_heaterPwmRunning) {
        ledcWrite((uint8_t)OVEN_HEATER, 0);
        ledcDetach((uint8_t)OVEN_HEATER);
        g_heaterPwmRunning = false;
        T15_INFO(t15_log::TEST_2_2, "Heater PWM detached (stopped)\n");
    }
#else
    static constexpr uint8_t HEATER_PWM_CHANNEL = 0;
    if (g_heaterPwmRunning) {
        ledcWrite(HEATER_PWM_CHANNEL, 0);
        ledcDetachPin(OVEN_HEATER);
        g_heaterPwmRunning = false;
        T15_INFO(t15_log::TEST_2_2, "Heater PWM detached (stopped)\n");
    }
#endif

    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);
}

static bool heaterPwmStart(uint8_t dutyPercent) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);

    (void)ledcDetach((uint8_t)OVEN_HEATER);
    g_heaterPwmRunning = false;

    const bool ok = ledcAttach((uint8_t)OVEN_HEATER,
                               (uint32_t)HEATER_PWM_FREQ_HZ,
                               (uint8_t)HEATER_PWM_RES_BITS);

    if (!ok) {
        T15_ERROR(t15_log::TEST_2_2,
                  "Heater ledcAttach FAILED (GPIO=%d, Freq=%luHz, Res=%dbit)\n",
                  OVEN_HEATER,
                  (unsigned long)HEATER_PWM_FREQ_HZ,
                  (int)HEATER_PWM_RES_BITS);
        pinMode(OVEN_HEATER, OUTPUT);
        digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);
        g_heaterPwmRunning = false;
        return false;
    }

    g_heaterPwmRunning = true;

    const uint32_t duty = heaterDutyFromPercent(dutyPercent);
    ledcWrite((uint8_t)OVEN_HEATER, 0);
    delay(2);
    ledcWrite((uint8_t)OVEN_HEATER, duty);
    delay(2);
    ledcWrite((uint8_t)OVEN_HEATER, duty);

    T15_INFO(t15_log::TEST_2_2,
             "Heater PWM attached. GPIO=%d Freq=%luHz Duty=%u%%\n",
             OVEN_HEATER,
             (unsigned long)HEATER_PWM_FREQ_HZ,
             (unsigned)dutyPercent);
    return true;
#else
    static constexpr uint8_t HEATER_PWM_CHANNEL = 0;

    ledcSetup(HEATER_PWM_CHANNEL, HEATER_PWM_FREQ_HZ, HEATER_PWM_RES_BITS);
    ledcAttachPin(OVEN_HEATER, HEATER_PWM_CHANNEL);
    g_heaterPwmRunning = true;

    const uint32_t duty = heaterDutyFromPercent(dutyPercent);
    ledcWrite(HEATER_PWM_CHANNEL, 0);
    delay(2);
    ledcWrite(HEATER_PWM_CHANNEL, duty);
    delay(2);
    ledcWrite(HEATER_PWM_CHANNEL, duty);

    T15_INFO(t15_log::TEST_2_2,
             "Heater PWM attached. GPIO=%d CH=%u Freq=%luHz Duty=%u%%\n",
             OVEN_HEATER,
             (unsigned)HEATER_PWM_CHANNEL,
             (unsigned long)HEATER_PWM_FREQ_HZ,
             (unsigned)dutyPercent);
    return true;
#endif
}

// --------------------------------------------------------------------
// Chamber measurement using manufacturer NTC table
// NOTE:
// Symbol names below follow the common project naming style.
// If your table header uses slightly different symbol names,
// adjust only this function.
// --------------------------------------------------------------------
static void sampleChamberTemperature() {
    if (!g_adsOk) {
        return;
    }

    g_rawChamber = ads.readADC_SingleEnded(1); // A1 = Chamber
    g_cha_mV = ntc::ads_raw_to_mV(g_rawChamber);
    g_cha_ohm = ntc::voltage_to_resistance_ohm(g_cha_mV, VREF_MV, RFIXED_CHA_OHM, NTC_TO_GND);

    g_cha_dC = ntc::calc_temp_from_ads_raw_dC(
        g_rawChamber,
        //        ntc::kNtcTable_10k_ioveo_036HS05201,
        //        ntc::kNtcTable_10k_ioveo_036HS05201_Count,
        ntc::kNtc10k_Chamber_Table,
        ntc::kNtc10k_Chamber_TableCount,
        //        ntc::NtcTableMode::RESISTANCE_OHM,
        ntc::NtcTableMode::Resistance_Ohm,
        VREF_MV,
        RFIXED_CHA_OHM,
        NTC_TO_GND);

    g_tempChamberC = (g_cha_dC == ntc::TEMP_INVALID_DC) ? NAN : (g_cha_dC / 10.0f);
}

// --------------------------------------------------------------------
// Test 2.2: Heater OFF runout / tau estimation
// Chamber-only because Hotspot is currently unavailable.
// --------------------------------------------------------------------
static void updateTest22(const uint32_t now) {
    const bool door_open = isDoorOpen();

    // Hard safety gate: door open means heater OFF always
    if (door_open) {
        if (g_heaterPwmRunning) {
            heaterPwmStop();
        }

        // stay idle until user closes door
        if (g_test22State != Test22State::IDLE) {
            g_test22State = Test22State::IDLE;
            g_test22Started = false;
            T15_WARN(t15_log::TEST_2_2, "Door OPEN -> test paused / heater forced OFF\n");
        }
        return;
    }

    if (isnan(g_tempChamberC)) {
        return;
    }

    // Auto-start the test once door is closed
    if (!g_test22Started) {
        g_test22Started = true;
        g_test22State = Test22State::HEATING_TO_TARGET;
        g_chamberAtOff_dC = ntc::TEMP_INVALID_DC;
        g_peakChamber_dC = ntc::TEMP_INVALID_DC;
        g_peakAtMs = 0;
        g_coolingConfirmCount = 0;

        heaterPwmStart(HEATER_TEST_DUTY_PERCENT);

        T15_INFO(t15_log::TEST_2_2,
                 "Test 2.2 START: heating chamber to target=%dC with duty=%u%%\n",
                 (int)SIM_TARGET_TEMP,
                 (unsigned)HEATER_TEST_DUTY_PERCENT);
    }

    switch (g_test22State) {
    case Test22State::IDLE:
        break;

    case Test22State::HEATING_TO_TARGET: {
        if (g_tempChamberC >= (float)SIM_TARGET_TEMP) {
            heaterPwmStop();
            g_heaterOffMs = now;
            g_chamberAtOff_dC = g_cha_dC;
            g_peakChamber_dC = g_cha_dC;
            g_peakAtMs = now;
            g_coolingConfirmCount = 0;
            g_test22State = Test22State::RUNOUT_AFTER_OFF;

            T15_INFO(t15_log::TEST_2_2,
                     "Heater OFF at Tch=%.2fC rawCh=%d mV=%ld Ohm=%ld\n",
                     g_tempChamberC,
                     (int)g_rawChamber,
                     (long)g_cha_mV,
                     (long)g_cha_ohm);
        }
        break;
    }

    case Test22State::RUNOUT_AFTER_OFF: {
        // track peak after heater off
        if (g_cha_dC > g_peakChamber_dC) {
            g_peakChamber_dC = g_cha_dC;
            g_peakAtMs = now;
            g_coolingConfirmCount = 0;
        } else {
            // after several consecutive non-increasing samples, call it peak reached
            if (g_peakChamber_dC != ntc::TEMP_INVALID_DC &&
                g_cha_dC < g_peakChamber_dC) {
                if (g_coolingConfirmCount < 255) {
                    g_coolingConfirmCount++;
                }
            }
        }

        // with 250ms sampling, 8 confirms = ~2s of cooling
        if (g_coolingConfirmCount >= 8) {
            const float offC = (g_chamberAtOff_dC == ntc::TEMP_INVALID_DC) ? NAN : (g_chamberAtOff_dC / 10.0f);
            const float peakC = (g_peakChamber_dC == ntc::TEMP_INVALID_DC) ? NAN : (g_peakChamber_dC / 10.0f);
            const float overshootC = (isnan(offC) || isnan(peakC)) ? NAN : (peakC - offC);
            const uint32_t timeToPeakMs = (g_peakAtMs >= g_heaterOffMs) ? (g_peakAtMs - g_heaterOffMs) : 0;

            T15_INFO(t15_log::TEST_2_2,
                     "Peak reached: Tch_peak=%.2fC overshoot=%.2fC time_to_peak=%lums (~%lus)\n",
                     peakC,
                     overshootC,
                     (unsigned long)timeToPeakMs,
                     (unsigned long)(timeToPeakMs / 1000UL));

            g_test22State = Test22State::PEAK_REACHED;
        }

        break;
    }

    case Test22State::PEAK_REACHED:
        // no automatic restart; keep test result stable until reboot
        break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!udp::begin("CLIENT")) {
        Serial.println("[T15] UDP init FAILED");
    }

    T15_INFO(t15_log::BASE,
             "Boot Step 3A.1: T15-2.2 chamber-only runout test, manufacturer table enabled\n");

    initDoor();
    initI2CAndADS();

    // Start safe
    heaterPwmStop();

    T15_INFO(t15_log::BASE,
             "Setup complete. Close door to start T15-2.2 runout measurement\n");
}

void loop() {
    const uint32_t now = millis();

    if (now - g_lastSampleMs >= SAMPLE_INTERVAL_MS) {
        g_lastSampleMs = now;
        sampleChamberTemperature();
        updateTest22(now);
    }

    if (now - g_lastLogMs >= LOG_INTERVAL_MS) {
        g_lastLogMs = now;

        const bool door_open = isDoorOpen();
        const char *stateStr = "IDLE";
        switch (g_test22State) {
        case Test22State::IDLE:
            stateStr = "IDLE";
            break;
        case Test22State::HEATING_TO_TARGET:
            stateStr = "HEATING";
            break;
        case Test22State::RUNOUT_AFTER_OFF:
            stateStr = "RUNOUT";
            break;
        case Test22State::PEAK_REACHED:
            stateStr = "PEAK_REACHED";
            break;
        }

        T15_INFO(t15_log::TEST_2_2,
                 "Door=%s state=%s rawCh=%d cha_mV=%ld cha_Ohm=%ld Tch=%.2fC heater=%s\n",
                 door_open ? "OPEN" : "CLOSED",
                 stateStr,
                 (int)g_rawChamber,
                 (long)g_cha_mV,
                 (long)g_cha_ohm,
                 g_tempChamberC,
                 g_heaterPwmRunning ? "ON" : "OFF");
    }
}
