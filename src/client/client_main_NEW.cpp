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

#include "ClientComm.h"
#include "pins_client.h"
#include "protocol.h"
#include <Arduino.h>

// -------------------------
// UART (HOST <-> CLIENT link)
// -------------------------
constexpr uint32_t LINK_BAUDRATE = 115200;
// constexpr uint8_t BUILDIN_LED_PIN = 2; // Onboard LED pin (if available)

// Serial2 = GPIO17 (TX2), GPIO16 (RX2)
HardwareSerial &linkSerial = Serial2;

// ClientComm must be constructed with RX/TX pins (per your requirement).
ClientComm clientComm(linkSerial, CLIENT_RX2, CLIENT_TX2);

// ADC input used for "temperature analog raw"
// constexpr int PIN_ADC0 = 36; // GPIO36 (input-only, OK for ADC)

// Map bit index 0..7 to GPIO pins
static constexpr int OUT_PINS[8] = {
    PIN_CH0, PIN_CH1, PIN_CH2, PIN_CH3,
    PIN_CH4, PIN_CH5, PIN_CH6, PIN_CH7};

// -------------------------
// Optional MAX6675 (K-Type thermocouple)
// -------------------------
// If you want MAX6675 integration here, define pins below to match your wiring.
// If you don't use MAX6675 right now, set ENABLE_MAX6675=0.
#ifndef ENABLE_MAX6675
#define ENABLE_MAX6675 1
#endif

#if ENABLE_MAX6675
#include <max6675.h>

// TODO: Set these to your real MAX6675 wiring pins on the WROOM.
// Common wiring: SCK=18, CS=5, SO=19  (BUT DO NOT ASSUME; set them correctly!)
#ifndef MAX6675_SCK_PIN
#define MAX6675_SCK_PIN 18
#endif
#ifndef MAX6675_CS_PIN
#define MAX6675_CS_PIN 5
#endif
#ifndef MAX6675_SO_PIN
#define MAX6675_SO_PIN 19
#endif

static MAX6675 g_thermocouple(MAX6675_SCK_PIN, MAX6675_CS_PIN, MAX6675_SO_PIN);
#endif

// -------------------------
// Helpers
// -------------------------
static void applyOutputs(uint16_t mask) {
    // Apply only bits 0..7 to real GPIO pins
    for (int i = 0; i < 8; ++i) {
        const int level = (mask & (1u << i)) ? HIGH : LOW;
        digitalWrite(OUT_PINS[i], level);
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
    RAW("[outputsChangedCallback] outputsMask=0x%16d\n", newMask);
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
    Serial.println("=== CLIENT START ===");
    Serial.println("UART link: Serial2 (RX2=GPIO16, TX2=GPIO17)");
    Serial.println("Supported HOST frames:");
    Serial.println("  H;SET;XXXX");
    Serial.println("  H;UPD;SSSS;CCCC");
    Serial.println("  H;TOG;TTTT");
    Serial.println("  H;GET;STATUS");
    Serial.println("  H;PING");
    Serial.println();
    Serial.println("Outputs (CH0..CH7) are mapped to bits 0..7 of outputsMask.");
    Serial.print("ADC0 pin: ");
    Serial.println(PIN_ADC0);
#if ENABLE_MAX6675
    Serial.print("MAX6675 enabled. Pins: SCK=");
    Serial.print((int)MAX6675_SCK_PIN);
    Serial.print(" CS=");
    Serial.print((int)MAX6675_CS_PIN);
    Serial.print(" SO=");
    Serial.println((int)MAX6675_SO_PIN);
#else
    Serial.println("MAX6675 disabled.");
    C
#endif
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    printStartupInfo();

    // Buildin LED pin
    pinMode(PIN_BOARD_LED, OUTPUT);
    digitalWrite(PIN_BOARD_LED, LOW);

    // Configure outputs
    for (int i = 0; i < 8; ++i) {
        pinMode(OUT_PINS[i], OUTPUT);
        digitalWrite(OUT_PINS[i], LOW);
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

void loop() {
    // Process link UART + protocol frames
    clientComm.loop();
}
