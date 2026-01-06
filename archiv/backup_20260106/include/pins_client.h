#pragma once

// -------------------------------------------
// Example GPIO pins (REPLACE with real pins)
// -------------------------------------------
constexpr int PIN_CH0 = 32; // GPIO32
constexpr int PIN_CH1 = 33; // GPIO33
constexpr int PIN_CH2 = 25; // GPIO25
constexpr int PIN_CH3 = 26; // GPIO26
constexpr int PIN_CH4 = 27; // GPIO27
constexpr int PIN_CH5 = 14; // GPIO14
constexpr int PIN_CH6 = 12; // GPIO12
constexpr int PIN_CH7 = 13; // GPIO13

constexpr int PIN_CH8 = 04;  // GPIO04
constexpr int PIN_CH9 = 0;   // GPIO0
constexpr int PIN_CH10 = 2;  // GPIO2
constexpr int PIN_CH11 = 15; // GPIO15

// -------------------------------------------
// Analog input pins (FOR CH12–CH15)
// -------------------------------------------
constexpr int PIN_ADC0 = 36; // ADC0
constexpr int PIN_ADC1 = 39; // ADC3
constexpr int PIN_ADC2 = 34; // ADC6
constexpr int PIN_ADC3 = 35; // ADC7

// -------------------------------------------
// MAX6675 thermocouple pins (example)
// -------------------------------------------
constexpr int PIN_MAX6675_SCK = 18; // GPIO18 (SPI_SCK)
constexpr int PIN_MAX6675_CS = 5;   // GPIO5 (SPI_SS)
constexpr int PIN_MAX6675_SO = 19;  // GPIO19 (SPI(MISO))

// -------------------------------------------
// Internel Board-LED
// -------------------------------------------
constexpr int PIN_BOARD_LED = 2;

// -------------------------------------------
// HOST-Mapping GPIO -> Logische Namen
// -------------------------------------------
constexpr int OVEN_FAN12V = PIN_CH0;
constexpr int OVEN_FAN230V = PIN_CH1;
constexpr int OVEN_FAN230V_SLOW = PIN_CH2;
constexpr int OVEN_LAMP = PIN_CH3;
constexpr int OVEN_SILICAT_MOTOR = PIN_CH4;
constexpr int OVEN_DOOR_SENSOR = PIN_CH5;
constexpr int OVEN_HEATER = PIN_CH6;

// ADC-Input
constexpr int OVEN_TEMP1_PORT1 = PIN_ADC0;
constexpr int OVEN_TEMP_KTYPE = PIN_ADC1;
