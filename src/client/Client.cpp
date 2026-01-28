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
static const char *bitmask8_to_str(uint16_t mask) {
    static char buf[9];
    for (int i = 0; i < 8; ++i) {
        const uint8_t bit = 1u << (7 - i); // print CH7..CH0
        buf[i] = (mask & bit) ? '1' : '0';
    }
    buf[8] = '\0';
    return buf;
}

static bool parse_hex4_at(const String &s, int start, uint16_t &out) {
    if (start < 0 || start + 4 > (int)s.length()) {
        return false;
    }
    uint16_t v = 0;
    for (int i = 0; i < 4; ++i) {
        char c = s[start + i];
        uint8_t n;
        if (c >= '0' && c <= '9') {
            n = (uint8_t)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            n = (uint8_t)(10 + (c - 'A'));
        } else if (c >= 'a' && c <= 'f') {
            n = (uint8_t)(10 + (c - 'a'));
        } else {
            return false;
        }
        v = (uint16_t)((v << 4) | n);
    }
    out = v;
    return true;
}

static bool isDoorOpen() {
    const int v = digitalRead(OVEN_DOOR_SENSOR);
    // CLIENT_INFO("DOOR_STATE: %s", v == HIGH ? "OPEN" : "CLOSED");
    // return DOOR_OPEN_IS_HIGH ? (v == HIGH) : (v == LOW);

    static bool last = false;
    bool now = DOOR_OPEN_IS_HIGH ? (v == HIGH) : (v == LOW);
    if (now != last) {
        CLIENT_INFO("[DOOR] state=%s (level=%d)\n", now ? "OPEN" : "CLOSED", v);
        last = now;
    }
    return now;
}

// static void applyOutputs(uint16_t mask) {
//     const bool doorOpen = isDoorOpen();

//     for (int i = 0; i < 8; ++i) {
//         const bool requestedOn = (mask & (1u << i)) != 0;

//         // HEATER: requires PWM/toggle, not DC
//         if (i == HEATER_BIT_INDEX) {
//             const bool heaterAllowed = requestedOn && !doorOpen; // stop heater if door open
//             heaterPwmEnable(heaterAllowed);
//             continue;
//         }

//         // MOTOR: only allowed if door is CLOSED
//         if (i == MOTOR_BIT_INDEX) {
//             const bool motorAllowed = requestedOn && !doorOpen;
//             digitalWrite(OUT_PINS[i], motorAllowed ? HIGH : LOW);
//             continue;
//         }

//         // T10.1.36 - Door-saftey issues
//         if (i == OUTPUT_BIT_MASK_8BIT::BIT_DOOR) {
//             continue; // DOOR is input-only
//         }
//         // All others: direct mapping
//         digitalWrite(OUT_PINS[i], requestedOn ? HIGH : LOW);
//     }
// }

static uint16_t g_effectiveMask = 0;

// Überarbeitet in T10.1.36
static void applyOutputs(uint16_t requestedMask) {
    const bool doorOpen = isDoorOpen();
    uint16_t eff = requestedMask;

    // Never treat DOOR as output
    eff &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);

    // Gate heater + motor if door open
    if (doorOpen) {
        eff &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
        eff &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_SILICA_MOTOR);
        // T10.1.36: FAN230 must OFF when door open
        eff &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_FAN230V);
        // FAN230V_SLOW stays "any" => do not touch
    }

    // Apply eff to physical pins (skip DOOR)
    for (int i = 0; i < 8; ++i) {
        if (i == OUTPUT_BIT_MASK_8BIT::BIT_DOOR) {
            continue;
        }
        const bool on = (eff & (1u << i)) != 0;

        if (i == HEATER_BIT_INDEX) {
            heaterPwmEnable(on);
            continue;
        }
        if (i == MOTOR_BIT_INDEX) {
            digitalWrite(OUT_PINS[i], on ? HIGH : LOW);
            continue;
        }

        digitalWrite(OUT_PINS[i], on ? HIGH : LOW);
    }

    g_effectiveMask = eff;
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
// T10.1.28 Door-Bugfix
static void fillStatusCallback(ProtocolStatus &st) {
    // Start from the last applied outputs (CH0..CH7 etc.)
    // st.outputsMask = clientComm.getOutputsMask();
    // neu in T10.1.36
    st.outputsMask = g_effectiveMask;

    // Door is an input (CH5 -> bit5). OPEN=HIGH, CLOSED=LOW.
    const bool door_open = isDoorOpen();
    if (door_open) {
        st.outputsMask |= (1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    } else {
        st.outputsMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    }

    // Raw ADC reading (typical 0..4095)
    st.adcRaw[0] = (uint16_t)analogRead(PIN_ADC0);

    // Not used yet
    st.adcRaw[1] = 0;
    st.adcRaw[2] = 0;
    st.adcRaw[3] = 0;

    // Optional MAX6675
    st.tempRaw = readTempRaw_QuarterC();
}

// static void fillStatusCallback(ProtocolStatus &st) {
//     st.outputsMask = clientComm.getOutputsMask();

//     // Raw ADC reading (typical 0..4095)
//     st.adcRaw[0] = (uint16_t)analogRead(PIN_ADC0);

//     // Not used yet
//     st.adcRaw[1] = 0;
//     st.adcRaw[2] = 0;
//     st.adcRaw[3] = 0;

//     // Optional MAX6675
//     st.tempRaw = readTempRaw_QuarterC();
// }

// Apply outputs immediately when mask changes
static void outputsChangedCallback(uint16_t newMask) {
    applyOutputs(newMask);

    // Debug (USB serial only)
    RAW("[outputsChangedCallback] outputsMask=0x%04X; BitMask: %s\n", newMask, bitmask8_to_str(newMask));
}

// Debug outgoing frames (optional)
// static void txLineCallback(const String &line, const String &dir) {
//     RAW("[CLIENT] [%s]: %s\n", dir.c_str(), line.c_str());
// }

// Debug outgoing frames (optional)
static void txLineCallback(const String &line, const String &dir) {
    uint16_t mask = 0;
    bool hasMask = false;

    // We only append BitMask for known frames that carry a 16-bit mask.
    // Expected formats:
    // - C;ACK;SET;MMMM
    // - C;ACK;UPD;MMMM
    // - C;ACK;TOG;MMMM
    // - C;STATUS;MMMM;A0;A1;A2;A3;TEMP

    const int idxAckSet = line.indexOf("C;ACK;SET;");
    if (idxAckSet >= 0) {
        const int hexPos = idxAckSet + (int)strlen("C;ACK;SET;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxAckUpd = (!hasMask) ? line.indexOf("C;ACK;UPD;") : -1;
    if (idxAckUpd >= 0) {
        const int hexPos = idxAckUpd + (int)strlen("C;ACK;UPD;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxAckTog = (!hasMask) ? line.indexOf("C;ACK;TOG;") : -1;
    if (idxAckTog >= 0) {
        const int hexPos = idxAckTog + (int)strlen("C;ACK;TOG;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxStatus = (!hasMask) ? line.indexOf("C;STATUS;") : -1;
    if (idxStatus >= 0) {
        const int hexPos = idxStatus + (int)strlen("C;STATUS;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    if (hasMask) {
        RAW("[CLIENT] [%s]: BitMask: (%s) => %s;",
            dir.c_str(), bitmask8_to_str(mask), line.c_str());
    } else {
        RAW("[CLIENT] [%s]: %s", dir.c_str(), line.c_str());
    }
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
            // ausgebaut wegen "Log-Hygiene"
            // RAW(".");
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
    if (enable) {
        if (!g_heaterPwmRunning) {
            const bool ok = ledcAttach((uint8_t)HEATER_PWM_GPIO,
                                       (uint32_t)HEATER_PWM_FREQ_HZ,
                                       (uint8_t)HEATER_PWM_RES_BITS);

            if (!ok) {
                CLIENT_ERR("[HEATER] ledcAttach FAILED (GPIO=%d, Freq=%dHz, Res=%dbit)\n",
                           HEATER_PWM_GPIO, HEATER_PWM_FREQ_HZ, HEATER_PWM_RES_BITS);

                // Fail-safe: force safe output level
                pinMode(HEATER_PWM_GPIO, OUTPUT);
                digitalWrite(HEATER_PWM_GPIO, HEATER_SAFE_LEVEL);
                g_heaterPwmRunning = false;
                return;
            }

            g_heaterPwmRunning = true;

            CLIENT_INFO("[HEATER] PWM attached. GPIO=%d, Freq=%dHz, Duty=%lu\n",
                        HEATER_PWM_GPIO, HEATER_PWM_FREQ_HZ, (unsigned long)duty);

            // ------------------------------------------------------------------
            // IMPORTANT: deterministic PWM start (Kick)
            // ------------------------------------------------------------------
            // 1) Force known LOW duty
            ledcWrite((uint8_t)HEATER_PWM_GPIO, 0);

            // 2) Give LEDC / GPIO matrix time to settle
            delayMicroseconds(200);
        }

        // 3) Apply real duty
        ledcWrite((uint8_t)HEATER_PWM_GPIO, duty);

        // 4) Optional second kick (harmless, but fixes "first-write lost" cases)
        delayMicroseconds(50);
        ledcWrite((uint8_t)HEATER_PWM_GPIO, duty);

    } else {
        if (g_heaterPwmRunning) {
            ledcWrite((uint8_t)HEATER_PWM_GPIO, 0);
            ledcDetach((uint8_t)HEATER_PWM_GPIO);
            g_heaterPwmRunning = false;
            CLIENT_INFO("[HEATER] PWM detached (stopped)\n");
        }

        pinMode(HEATER_PWM_GPIO, OUTPUT);
        digitalWrite(HEATER_PWM_GPIO, HEATER_SAFE_LEVEL);
    }

// #if ESP_ARDUINO_VERSION_MAJOR >= 3
//     // Core 3.x: pin-based API
//     if (enable) {
//         if (!g_heaterPwmRunning) {
//             // Returns bool; you may log it if desired
//             (void)ledcAttach((uint8_t)HEATER_PWM_GPIO, (uint32_t)HEATER_PWM_FREQ_HZ, (uint8_t)HEATER_PWM_RES_BITS);
//             g_heaterPwmRunning = true;
//             CLIENT_INFO("[HEATER] g_heaterPwmRunning now running. GPIO: %d, Freq: %dHz, Duty:%d%\n",
//                         HEATER_PWM_GPIO,
//                         HEATER_PWM_FREQ_HZ,
//                         duty);
//         }
//         ledcWrite((uint8_t)HEATER_PWM_GPIO, duty);

//         // DEBUG (temporary): prove the pin is actually connected where we measure
//         // WARNING: this disturbs PWM, only for a quick sanity test.
//         pinMode(HEATER_PWM_GPIO, OUTPUT);
//         digitalWrite(HEATER_PWM_GPIO, HIGH);
//         delay(50);
//         digitalWrite(HEATER_PWM_GPIO, LOW);
//         delay(50);

//     } else {
//         if (g_heaterPwmRunning) {
//             ledcWrite((uint8_t)HEATER_PWM_GPIO, 0);
//             ledcDetach((uint8_t)HEATER_PWM_GPIO);
//             g_heaterPwmRunning = false;
//             CLIENT_INFO("[HEATER] g_heaterPwmRunning now stopped\n");
//         }
//         pinMode(HEATER_PWM_GPIO, OUTPUT);
//         digitalWrite(HEATER_PWM_GPIO, HEATER_SAFE_LEVEL);
//     }
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
        //     if (OUT_PINS[i] == OVEN_DOOR_SENSOR) {
        //         // TP10.1 Door-Bugfix
        //         pinMode(OUT_PINS[i], INPUT_PULLUP);
        //         digitalWrite(OUT_PINS[i], HIGH);
        //         CLIENT_INFO("[IO] OVEN_DOOR_SENSOR PIN init: GPIO=%d\n", i);

        //     } else {
        //         pinMode(OUT_PINS[i], OUTPUT);
        //         digitalWrite(OUT_PINS[i], LOW);
        //         CLIENT_INFO("[IO] OUTPUT-PIN init: GPIO=%d\n", OVEN_DOOR_SENSOR);
        //     }
        if (OUT_PINS[i] == OVEN_DOOR_SENSOR) {
            pinMode(OUT_PINS[i], INPUT_PULLUP);
            CLIENT_INFO("[IO] INPUT_PULLUP init: GPIO=%d (DOOR)\n", OUT_PINS[i]);
        } else {
            pinMode(OUT_PINS[i], OUTPUT);
            digitalWrite(OUT_PINS[i], LOW);
            CLIENT_INFO("[IO] OUTPUT init: GPIO=%d\n", OUT_PINS[i]);
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

    // TP10.1 (Door-Bugfix)
    const bool door_open = (digitalRead(OVEN_DOOR_SENSOR) != 0);
    CLIENT_INFO("[IO] DOOR init done: GPIO=%d INPUT_PULLUP level=%d (%s)\n",
                OVEN_DOOR_SENSOR,
                door_open ? 1 : 0,
                door_open ? "OPEN" : "CLOSED");
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