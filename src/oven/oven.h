#pragma once
#include <Arduino.h>
#include "log_oven.h"
#include "PCF8574.h"

#define OVEN_P0_FAN12V P0        // PIN 5 auf Stecker
#define OVEN_P1_FAN230V P1       // PIN 7 auf Stecker
#define OVEN_P2_FAN230V_SLOW P2  // PIN 10 auf Stecker
#define OVEN_P3_LAMP P3          // PIN 8 auf Stecker
#define OVEN_P4_SILICAT_MOTOR P4 // PIN 9 auf Stecker
#define OVEN_P5_DOOR_SENSOR P5   // PIN 12 auf Stecker
#define OVEN_P6_HEATER P6        // PINT 6 auf stecker
class Oven
{
public:
    Oven(uint8_t scl, uint8_t sda, uint8_t addr = 0x20);
    ~Oven();
    void begin();
    void setTemperature(float temperature);
    float getTemperature() const;
    void setTargetTemperature(float target);
    float getTargetTemperature() const;

    void fan12VOn();
    void fan12VOff();
    void fan230VOn();
    void fan230VOff();
    void fan230VSlowOn();
    void fan230VSlowOff();
    void lampOn();
    void lampOff();
    void heaterOn();
    void heaterOff();
    void silicatMotorOn();
    void silicatMotorOff();
    void setDoorOpen();
    void setDoorClosed();
    bool isDoorOpen() const;
    bool isDoorClosed() const;

private:
    uint8_t _scl;
    uint8_t _sda;
    uint8_t _addr;
    bool _initialized = false;
    bool _fan12VState = false;
    bool _fan230VState = false;
    bool _lampState = false;
    bool _silicatMotorState = false;
    bool _doorClosed = false; // true = closed, false = open
    bool _doorOpen = false;   // true = open, false = closed
    bool _heaterState = false;
    float _currentTemperature = 0.0f;
    float _targetTemperature = 0.0f;

    PCF8574 *_pcf;
};
