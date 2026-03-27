#pragma once

#include "HostComm.h"
#include "log_oven.h"
#include "output_bitmask.h"
#include "protocol.h"
#include <Arduino.h>
#include <cstring>
#include <stdbool.h>
#include <stdint.h>

/*
================================================================================
OVEN MODULE (Host-side "Business Logic")
================================================================================

This module is the *host-side* controller for the filament dryer application.

Key idea (T6 architecture):
- The ESP32-S3 (Host/UI) does NOT directly drive hardware outputs.
- The ESP32-WROOM (Client/Powerboard) is authoritative for:
    - output states (heater/fans/motor/lamp)
    - sensor inputs (door, temperature, etc.)
- The Host communicates over UART (ASCII protocol with CRLF) via HostComm.

"Single Source of Truth":
- The UI renders ONLY from OvenRuntimeState (oven_get_runtime_state()).
- OvenRuntimeState actuator booleans are updated ONLY when telemetry arrives
  from the Client (C;STATUS), never by "wishful thinking" on the Host.

================================================================================
*/

enum class OVEN_CONNECTOR : uint16_t {
    FAN12V = 1u << OUTPUT_BIT_MASK_8BIT::BIT_FAN12V,              // CH0
    FAN230V = 1u << OUTPUT_BIT_MASK_8BIT::BIT_FAN230V,            // CH1
    LAMP = 1u << OUTPUT_BIT_MASK_8BIT::BIT_LAMP,                  // CH2
    SILICAT_MOTOR = 1u << OUTPUT_BIT_MASK_8BIT::BIT_SILICA_MOTOR, // CH3
    FAN230V_SLOW = 1u << OUTPUT_BIT_MASK_8BIT::BIT_FAN230V_SLOW,  // CH4
    DOOR_ACTIVE = 1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR,           // CH5 (GPIO14)  TP10.1 Door-Bugfix
    HEATER = 1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER               // CH6 (GPIO12) TP10.1 Door-Bugfix
};

// ----------------------------------------------------------------------------
// T7: High-level oven mode (host-side state machine)
// ----------------------------------------------------------------------------
enum class OvenMode : uint8_t {
    STOPPED = 0,
    RUNNING,
    WAITING,
    POST
};

// ----------------------------------------------------------------------------
// T7: POST fan behavior
// ----------------------------------------------------------------------------
enum class PostFanMode : uint8_t {
    FAST = 0, // FAN230V
    SLOW      // FAN230V_SLOW
};

// ----------------------------------------------------------------------------
// T7: POST configuration (preset-time plan) and runtime state
// ----------------------------------------------------------------------------
typedef struct
{
    bool active;
    uint16_t seconds;
    PostFanMode fanMode;
} PostConfig;

typedef struct
{
    bool active;
    uint16_t secondsRemaining;
    uint8_t stepIndex;
} PostRuntime;

// Host requests STATUS periodically
constexpr uint32_t kStatusPollIntervalMs = 500; // request STATUS every n ms

// ----------------------------------------------------------------------------
// Presets & Profiles
// ----------------------------------------------------------------------------
typedef struct
{
    const char *name;
    float dryTempC;
    uint16_t durationMin;
    bool rotaryOn;

    // T7: POST behavior (cooling etc.) – preset-time plan
    PostConfig post;

} FilamentPreset;

typedef struct
{
    uint32_t durationMinutes; // in minutes
    float targetTemperature;  // in Celsius
    int filamentId;           // FilamentPreset index
} OvenProfile;

/**
 * OvenRuntimeState:
 * The single UI-facing state snapshot.
 */
typedef struct
{
    uint32_t durationMinutes;
    uint32_t secondsRemaining;

    // T16/T13 naming cleanup:
    // - tempChamberC: authoritative control/UI temperature from client STATUS
    // - tempHotspotC: authoritative safety temperature from client STATUS
    // - tempCurrent / tempNtcC are temporary aliases for older UI paths
    float tempCurrent;     // legacy alias -> tempChamberC
    float tempChamberC;    // control/UI temperature (Chamber)
    float tempTarget;      // UI target
    float tempToleranceC;  // hysteresis band (host-side)
    bool hostOvertempActive;
    bool safetyCutoffActive; // T13: true when any safety cutoff is active
    float tempHotspotC;    // safety temperature (Hotspot)
    bool tempChamberValid;
    bool tempHotspotValid;

    int filamentId;
    char presetName[24];
    bool rotaryOn;

    // Remote truth (from client STATUS)
    bool fan12v_on;
    bool fan230_on;
    bool fan230_slow_on;
    bool heater_on;         // legacy UI alias -> request while RUNNING, else actual
    bool heater_request_on; // host decision / command intent
    bool heater_actual_on;  // client telemetry truth
    bool door_open;
    bool motor_on;
    bool lamp_on;

    // Safety gates for UI manual toggles
    bool fan230_manual_allowed;
    bool motor_manual_allowed;
    bool lamp_manual_allowed;

    // Communication diagnostics (host-side)
    uint32_t lastStatusAgeMs;
    uint32_t lastRxAnyAgeMs;
    uint32_t statusRxCount;
    uint32_t commErrorCount;

    // Communication / link diagnostics
    bool commAlive;
    bool linkSynced;

    // Host-side mode (not remote truth)
    OvenMode mode;

    // Backward-compat flag (temporary; will be removed once UI switches to mode)
    bool running;

    // T7: POST runtime state
    PostRuntime post;

    // Temporary legacy alias kept while older UI paths are migrated.
    float tempNtcC; // legacy alias -> tempHotspotC

} OvenRuntimeState;

// ----------------------------------------------------------------------------
// Preset list (immutable factory presets)
// ----------------------------------------------------------------------------
static constexpr FilamentPreset kPresets[] = {
    {"CUSTOM", 0.0f, 0, false, {false, 0, PostFanMode::SLOW}},
    {"SILICA", 105.0f, 90, true, {true, 60, PostFanMode::FAST}},
    {"ABS", 80.0f, 300, false, {true, 300, PostFanMode::FAST}},
    {"ASA", 82.5f, 300, false, {true, 300, PostFanMode::FAST}},
    {"PETG", 62.5f, 360, false, {true, 300, PostFanMode::SLOW}},
    {"PLA", 47.5f, 300, false, {true, 300, PostFanMode::SLOW}}, // 5
    {"TPU", 45.0f, 300, false, {true, 300, PostFanMode::FAST}},
    {"Spec-ASA-CF", 85.0f, 540, false, {true, 300, PostFanMode::FAST}},
    {"Spec-BVOH", 52.5f, 420, false, {true, 300, PostFanMode::FAST}},
    {"Spec-HIPS", 65.0f, 300, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PA(CF,PET,PH*)", 85.0f, 540, false, {true, 300, PostFanMode::FAST}}, // 10
    {"Spec-PC(CF/FR)", 85.0f, 540, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PC-ABS", 82.5f, 540, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PET-CF", 75.0f, 480, false, {true, 300, PostFanMode::SLOW}},
    {"Spec-PETG-CF", 70.0f, 480, false, {true, 300, PostFanMode::SLOW}},
    {"Spec-PETG-HF", 65.0f, 360, false, {true, 300, PostFanMode::SLOW}}, // 15
    {"Spec-PLA-CF", 55.0f, 360, false, {true, 300, PostFanMode::SLOW}},
    {"Spec-PLA-HT", 55.0f, 360, false, {true, 300, PostFanMode::SLOW}},
    {"Spec-PLA-WoodMetal", 45.0f, 300, false, {true, 300, PostFanMode::SLOW}},
    {"Spec-POM", 70.0f, 300, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PP", 55.0f, 300, false, {true, 300, PostFanMode::FAST}}, // 20
    {"Spec-PP-GF", 65.0f, 480, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PPS(+CF)", 85.0f, 540, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PVA", 50.0f, 480, false, {true, 300, PostFanMode::FAST}},
    {"Spec-PVDF-PPSU", 85.0f, 540, false, {true, 300, PostFanMode::FAST}},
    {"Spec-TPU 82A", 42.5f, 300, false, {true, 300, PostFanMode::FAST}}, // 25
    {"Spec-WOOD-Composite", 45.0f, 300, false, {true, 300, PostFanMode::FAST}},
};

static constexpr uint16_t kPresetCount =
    sizeof(kPresets) / sizeof(kPresets[0]);
static constexpr uint16_t OVEN_DEFAULT_PRESET_INDEX = 5; // PLA

static constexpr uint8_t UI_MIN_MINUTES = 1;

// Product defaults for the current host-side heater policy.
static constexpr float HOST_HEATER_HYSTERESIS_C = 1.5f;
static constexpr float HOST_TARGET_OVERSHOOT_CAP_C = 2.0f;
static constexpr float HOST_CHAMBER_MAX_C = 120.0f;
static constexpr float HOST_HOTSPOT_MAX_C = 140.0f;

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------
void oven_init(void);
void oven_tick(void);

void oven_start(void);
void oven_stop(void);
bool oven_is_running(void);
bool oven_is_alive(void);

void oven_get_runtime_state(OvenRuntimeState *stateOut);

void oven_command_toggle_motor_manual(void);
void oven_fan230_toggle_manual(void);
void oven_lamp_toggle_manual(void);

void oven_dbg_hw_toggle_by_index(int idx);
void oven_force_outputs_off(void);

void oven_pause_wait(void);
bool oven_resume_from_wait(void);
bool oven_is_waiting(void);

uint16_t oven_get_preset_count(void);
void oven_get_preset_name(uint16_t index, char *out, size_t out_len);
void oven_select_preset(uint16_t index);
int oven_get_current_preset_index(void);
const FilamentPreset *oven_get_preset(uint16_t index);

void oven_set_runtime_duration_minutes(uint16_t duration_min);
void oven_set_runtime_temp_target(uint16_t temp_c);

void oven_set_runtime_actuator_fan230(bool on);
void oven_set_runtime_actuator_fan230_slow(bool on);
void oven_set_runtime_actuator_heater(bool on);
void oven_set_runtime_actuator_motor(bool on);
void oven_set_runtime_actuator_lamp(bool on);

void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx);
void oven_comm_poll(void);

// END OF FILE
