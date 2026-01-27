//
// client_main.cpp
// ESP32-WROOM CLIENT (real hardware) using ClientComm + protocol callbacks.
//
// Purpose:
// - Communicate with HOST over UART2 (Serial2) on GPIO16 (RX2) / GPIO17 (TX2)
// - Apply digital outputs CH0..CH7 based on outputsMask bits 0..7
// - Provide STATUS with:
//     - outputsMask (current)
//     - adcRaw[0] = analogRead(PIN_ADC0) (raw ADC units)
//     - tempRaw    = optional MAX6675 temperature in 0.25°C steps (°C * 4)
//
// Notes:
// - This file is intended to be "production-like" and works with the updated protocol:
//     H;SET;XXXX
//     H;UPD;SSSS;CCCC
//     H;TOG;TTTT
//     H;GET;STATUS
//     H;PING
//   with responses:
//     C;ACK;SET;MMMM
//     C;ACK;UPD;MMMM
//     C;ACK;TOG;MMMM
//     C;STATUS;mask;a0;a1;a2;a3;tempRaw
//     C;PONG
//
// - Digital output pins must be OUTPUT-capable. (GPIO34/35/36/39 are input-only!)
//
// - Your uploaded pins_client.h currently contains a literal "..." line which would not compile.
//   This file therefore defines the required pins locally (CH0..CH7 + ADC0).
//   If your real pins_client.h is clean/complete, you may replace these definitions with:
//       #include "pins_client.h"
//

#include "Client.h"
#include "log_client.h"
// #include "pins_client.h"
#include <Arduino.h>

constexpr const char *VERSION = "V0.3 - 20260127";

static void heaterPwmEnable(bool enable);
static bool isDoorOpen();

// -------------------------
// Helpers
// -------------------------
// static void applyOutputs(uint16_t mask) {
//     // Apply only bits 0..7 to real GPIO pins
//     for (int i = 0; i < 8; ++i) {
//         const int level = (mask & (1u << i)) ? HIGH : LOW;
//         digitalWrite(OUT_PINS[i], level);
//     }
// }

static bool isDoorOpen() {
    const int v = digitalRead(OVEN_DOOR_SENSOR);
    return DOOR_OPEN_IS_HIGH ? (v == HIGH) : (v == LOW);
}

static void applyOutputs(uint16_t mask) {
    const bool doorOpen = isDoorOpen();

    for (int i = 0; i < 8; ++i) {
        const bool requestedOn = (mask & (1u << i)) != 0;

        // HEATER: requires PWM/toggle, not DC
        if (i == HEATER_BIT_INDEX) {
            const bool heaterAllowed = requestedOn && !doorOpen; // stop heater if door open
            heaterPwmEnable(heaterAllowed);
            continue;
        }

        // MOTOR: only allowed if door is CLOSED
        if (i == MOTOR_BIT_INDEX) {
            const bool motorAllowed = requestedOn && !doorOpen;
            digitalWrite(OUT_PINS[i], motorAllowed ? HIGH : LOW);
            continue;
        }

        // All others: direct mapping
        digitalWrite(OUT_PINS[i], requestedOn ? HIGH : LOW);
    }
}

static int16_t readTempRaw_QuarterC() {
    // tempRaw is defined as 0.25°C steps (tempRaw = °C * 4)
#if ENABLE_MAX6675
    const float c = g_thermocouple.readCelsius();

    // MAX6675 returns NAN if not connected / invalid; handle safely.
    if (isnan(c) || c < -100.0f || c > 1000.0f) {
        return 0;
    }

    // Round to nearest 0.25°C step
    const int32_t raw = (int32_t)lroundf(c * 4.0f);
    if (raw < INT16_MIN) {
        return INT16_MIN;
    }
    if (raw > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)raw;
#else
    return 0;
#endif
}

// Fill STATUS via callback (raw units only; no assumptions about conversion)
static void fillStatusCallback(ProtocolStatus &st) {
    st.outputsMask = clientComm.getOutputsMask();

    // Raw ADC reading (typical 0..4095)
    st.adcRaw[0] = (uint16_t)analogRead(PIN_ADC0);

    // Not used yet
    st.adcRaw[1] = 0;
    st.adcRaw[2] = 0;
    st.adcRaw[3] = 0;

    // Optional MAX6675
    st.tempRaw = readTempRaw_QuarterC();
}

// Apply outputs immediately when mask changes
static void outputsChangedCallback(uint16_t newMask) {
    applyOutputs(newMask);

    // Debug (USB serial only)
    RAW("[outputsChangedCallback] outputsMask=0x%04d\n", newMask);
}

// Debug outgoing frames (optional)
static void txLineCallback(const String &line, const String &dir) {
    RAW("[CLIENT] [%s]: %s\n", dir.c_str(), line.c_str());
}

void heartbeatLED_update() {
    static bool ledOn = false;
    static uint32_t nextToggleMs = 0;

    static uint8_t hbCount = 0;
    static uint32_t lastHBCount = 0;

    const uint8_t HB_LINE_BREAK = 80;
    const uint32_t PERIOD_MS = 5000; // 5 seconds (0.2 Hz)
    const uint32_t PULSE_MS = 350;   // LED ON time

    // Many built-in LEDs are active-low:
    const bool LED_ACTIVE_LOW = false;

    auto led_write = [&](bool on) {
        if (LED_ACTIVE_LOW) {
            digitalWrite(PIN_BOARD_LED, on ? LOW : HIGH);
        } else {
            digitalWrite(PIN_BOARD_LED, on ? HIGH : LOW);
        }
    };

    uint32_t now = millis();

    if (!ledOn && (int32_t)(now - nextToggleMs) >= 0) {
        led_write(true);
        ledOn = true;
        nextToggleMs = now + PULSE_MS;

        if (++hbCount >= HB_LINE_BREAK) {
            hbCount = 0;
            lastHBCount++;
            RAW(" (%05lu)\n", (unsigned long)lastHBCount);
        } else {
            RAW(".");
        }
    } else if (ledOn && (int32_t)(now - nextToggleMs) >= 0) {
        led_write(false);
        ledOn = false;
        nextToggleMs = now + (PERIOD_MS - PULSE_MS);
    }
}

static void printStartupInfo() {
    Serial.println();
    CLIENT_INFO("----------------------------------------------\n");
    CLIENT_INFO("- ESP32-WROOM CLIENT Hardware connector.   ---\n");
    CLIENT_INFO("----------------------------------------------\n");
    CLIENT_INFO("Version: %s\n\n", VERSION);

    CLIENT_INFO("UART link: Serial2 (RX2=GPIO16, TX2=GPIO17)\n");
    CLIENT_INFO("Supported HOST frames:\n");
    CLIENT_INFO("  H;SET;XXXX\n");
    CLIENT_INFO("  H;UPD;SSSS;CCCC\n");
    CLIENT_INFO("  H;TOG;TTTT\n");
    CLIENT_INFO("  H;GET;STATUS\n");
    CLIENT_INFO("  H;PING\n");
    CLIENT_INFO("\n");
    CLIENT_INFO("Outputs (CH0..CH7) are mapped to bits 0..7 of outputsMask.\n");
    CLIENT_INFO("ADC0 pin: %d\n", PIN_ADC0);
#if ENABLE_MAX6675
    CLIENT_INFO("MAX6675 enabled. Pins: SCK= %d, CS=%d, SO=%d\n",
                (int)MAX6675_SCK_PIN,
                (int)MAX6675_CS_PIN,
                (int)MAX6675_SO_PIN);
#else
    Serial.println("MAX6675 disabled.");
#endif
    Serial.println();
}

static uint32_t heaterDutyFromPercent(int percent) {
    const uint32_t maxDuty = (1u << HEATER_PWM_RES_BITS) - 1u;
    if (percent <= 0) {
        return 0;
    }
    if (percent >= 100) {
        return maxDuty;
    }
    // Round to nearest
    return (maxDuty * (uint32_t)percent + 50u) / 100u;
}

static void heaterPwmEnable(bool enable) {
    const uint32_t duty = heaterDutyFromPercent(HEATER_PWM_DUTY_PERCENT);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    // Core 3.x: pin-based API
    if (enable) {
        if (!g_heaterPwmRunning) {
            // Returns bool; you may log it if desired
            (void)ledcAttach((uint8_t)HEATER_PWM_GPIO, (uint32_t)HEATER_PWM_FREQ_HZ, (uint8_t)HEATER_PWM_RES_BITS);
            g_heaterPwmRunning = true;
            CLIENT_INFO("[HEATER] g_heaterPwmRunning now running. GPIO: %d, Freq: %dHz, Duty:%d%\n",
                        HEATER_PWM_GPIO,
                        HEATER_PWM_FREQ_HZ,
                        duty);
        }
        ledcWrite((uint8_t)HEATER_PWM_GPIO, duty);
    } else {
        if (g_heaterPwmRunning) {
            ledcWrite((uint8_t)HEATER_PWM_GPIO, 0);
            ledcDetach((uint8_t)HEATER_PWM_GPIO);
            g_heaterPwmRunning = false;
            CLIENT_INFO("[HEATER] g_heaterPwmRunning now stopped\n");
        }
        pinMode(HEATER_PWM_GPIO, OUTPUT);
        digitalWrite(HEATER_PWM_GPIO, HEATER_SAFE_LEVEL);
    }
#else
    // Core 2.x: channel-based API
    if (enable) {
        if (!g_heaterPwmRunning) {
            ledcSetup(HEATER_PWM_CHANNEL, HEATER_PWM_FREQ_HZ, HEATER_PWM_RES_BITS);
            ledcAttachPin(HEATER_PWM_GPIO, HEATER_PWM_CHANNEL);
            g_heaterPwmRunning = true;
        }
        ledcWrite(HEATER_PWM_CHANNEL, duty);
    } else {
        if (g_heaterPwmRunning) {
            ledcWrite(HEATER_PWM_CHANNEL, 0);
            ledcDetachPin(HEATER_PWM_GPIO);
            g_heaterPwmRunning = false;
        }
        pinMode(HEATER_PWM_GPIO, OUTPUT);
        digitalWrite(HEATER_PWM_GPIO, HEATER_SAFE_LEVEL);
    }
#endif
}

//----------------------------------------------------------------------------
// HELPER - END
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// setup
//----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);
    printStartupInfo();

    // Buildin LED pin
    pinMode(PIN_BOARD_LED, OUTPUT);
    digitalWrite(PIN_BOARD_LED, LOW);

    // Configure outputs
    for (int i = 0; i < 8; ++i) {
        if (OUT_PINS[i] == OVEN_DOOR_SENSOR) {
            pinMode(OUT_PINS[i], INPUT);
            digitalWrite(OUT_PINS[i], HIGH);
        } else {
            pinMode(OUT_PINS[i], OUTPUT);
            digitalWrite(OUT_PINS[i], LOW);
        }
    }

    // ADC pin (GPIO36 is input-only, correct)
    pinMode(PIN_ADC0, INPUT);
    // Initialize ClientComm UART (routes RX2/TX2 inside ClientComm as required)
    clientComm.begin(LINK_BAUDRATE);

    // Register callbacks
    clientComm.setOutputsChangedCallback(outputsChangedCallback);
    clientComm.setFillStatusCallback(fillStatusCallback);
    clientComm.setTxLineCallback(txLineCallback);
    clientComm.setHeartBeatCallback(heartbeatLED_update);
    // Ensure outputs are at known state
    outputsChangedCallback(0x0000);
}

//----------------------------------------------------------------------------
// LOOP
//
// please do not include anything here without knowing the consequences ;-)
//----------------------------------------------------------------------------
void loop() {
    // Process link UART + protocol frames
    clientComm.loop();
}

// EOF