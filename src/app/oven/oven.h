#pragma once
#include <Arduino.h>
#include "log_oven.h"
#include "PCF8575.h"

#define OVEN_P0_FAN12V P0        // PIN 5 auf Stecker
#define OVEN_P1_FAN230V P1       // PIN 7 auf Stecker
#define OVEN_P2_FAN230V_SLOW P2  // PIN 10 auf Stecker
#define OVEN_P3_LAMP P3          // PIN 8 auf Stecker
#define OVEN_P4_SILICAT_MOTOR P4 // PIN 9 auf Stecker
#define OVEN_P5_DOOR_SENSOR P5   // PIN 12 auf Stecker
#define OVEN_P6_HEATER P6        // PINT 6 auf stecker

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Drying profile configuration
typedef struct
{
    // total drying duration in minutes (HH:MM)
    uint32_t durationMinutes;
    float targetTemperature;
    int filamentId;
} OvenProfile;

typedef struct
{
    // configured profile duration in minutes (for UI/config)
    uint32_t durationMinutes;

    // countdown in seconds (for runtime + HH:MM:SS display)
    uint32_t secondsRemaining;

    float tempCurrent;
    float tempTarget;

    int filamentId;

    // Actuator states ...
    bool fan12v_on;
    bool fan230_on;
    bool fan230_slow_on;
    bool heater_on;
    bool door_open;
    bool motor_on;
    bool lamp_on;

    bool fan230_manual_allowed;
    bool motor_manual_allowed;
    bool lamp_manual_allowed;

    bool running;

} OvenRuntimeState;

// Initialization – called from setup()
void oven_init(void);

// Called in loop() to update timing, state machine, sensors, etc.
void oven_tick(void);

// Start/stop the drying process
void oven_start(void);
void oven_stop(void);
bool oven_is_running(void);

// Profile access
void oven_set_profile(const OvenProfile *profile);
void oven_get_profile(OvenProfile *profileOut);

// Current runtime state for UI
void oven_get_runtime_state(OvenRuntimeState *stateOut);

// Manual commands triggered by icon buttons on the main screen
void oven_command_toggle_fan230_manual(void);
void oven_command_toggle_motor_manual(void);
void oven_command_toggle_lamp_manual(void);