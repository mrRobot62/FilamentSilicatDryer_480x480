//-------------------------------------------------------
// Client
//
// based on ESP32-Wroom. Responsible to connect GPIO
// Pins based on protocoll.h / ClientComm.h
//
// Please read doc/client/ClientComm_V0.3_Documentation.md
//
// Control PowerBoard
//  - FAN12V
//  - FAN230V + FAN230V_SLOW
//  - HEATER (PWM-Signal)
//  - LAMP
//  - MOTOR (SilicaGel Motor)
//  - DOOR
//-------------------------------------------------------
#pragma once

#include "ClientComm.h"
#include "pins_client.h"
#include "protocol.h"

#include <Arduino.h>

// -------------------------
// UART (HOST <-> CLIENT link)
// -------------------------
constexpr uint32_t LINK_BAUDRATE = 115200;

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
// HEATER PWM (Powerboard requires toggle/PWM, not DC)
// -------------------------
constexpr int HEATER_BIT_INDEX = 6;         // OUT_PINS[6] = PIN_CH6 = GPIO12
constexpr int HEATER_PWM_GPIO = PIN_CH6;    // GPIO12
constexpr int HEATER_PWM_FREQ_HZ = 4000;    // measured requirement
constexpr int HEATER_PWM_DUTY_PERCENT = 50; // measured requirement
constexpr int HEATER_PWM_RES_BITS = 10;     // 8..12 ok; 10 is a good default
constexpr int HEATER_SAFE_LEVEL = LOW;      // safe idle level when OFF

// -------------------------
// DOOR gating
// -------------------------
constexpr bool DOOR_OPEN_IS_HIGH = true; // OPEN = 5V => HIGH
constexpr int MOTOR_BIT_INDEX = 3;       // OUT_PINS[3] = PIN_CH3 = GPIO26

#if !defined(ESP_ARDUINO_VERSION_MAJOR)
// Fallback if the macro is not available
#define ESP_ARDUINO_VERSION_MAJOR 0
#endif

#if ESP_ARDUINO_VERSION_MAJOR < 3
// Arduino-ESP32 Core 2.x uses channel-based API
constexpr int HEATER_PWM_CHANNEL = 0;
#endif

static bool g_heaterPwmRunning = false;
