#pragma once
#include "HostComm.h"
#include "log_oven.h"
#include "protocol.h"
#include <Arduino.h>
#include <cstring>
#include <stdbool.h>
#include <stdint.h>

enum class OVEN_CONNECTOR : uint16_t {
    FAN12V = 1u << 0,        // bit 0
    FAN230V = 1u << 1,       // bit 1
    FAN230V_SLOW = 1u << 2,  // bit 2
    SILICAT_MOTOR = 1u << 3, // bit 3
    HEATER = 1u << 4,        // bit 4
    LAMP = 1u << 5,          // bit 5
    DOOR_ACTIVE = 1u << 6    // bit 6
};

// Drying profile configuration
constexpr uint32_t kStatusPollIntervalMs = 2000; // request STATUS every 2 seconds

typedef struct
{
    const char *name;
    float dryTempC;
    uint16_t durationMin;
    bool rotaryOn; // SILICA => true
} FilamentPreset;

typedef struct
{
    uint32_t durationMinutes; // in minutes
    float targetTemperature;  // in Celsius
    int filamentId;           // FilamentPreset index
} OvenProfile;

typedef struct
{
    uint32_t durationMinutes;
    uint32_t secondsRemaining;

    float tempCurrent;
    float tempTarget;
    float tempToleranceC;

    int filamentId;
    char presetName[24]; // UI-ready text
    bool rotaryOn;

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

    // Communication diagnostics (host-side)
    bool commAlive;
    uint32_t lastStatusAgeMs;
    uint32_t statusRxCount;
    uint32_t commErrorCount;

} OvenRuntimeState;

static constexpr FilamentPreset kPresets[] = {
    // Name, Temp(C), Dur(min), Rotary
    {"CUSTOM", 0.0f, 0, false},
    {"SILICA", 105.0f, 90, true},
    {"ABS", 80.0f, 300, false},
    {"ASA", 82.5f, 300, false},
    {"PETG", 62.5f, 360, false},
    {"PLA", 47.5f, 300, false}, // 5
    {"TPU", 45.0f, 300, false},
    {"Spec-ASA-CF", 85.0f, 540, false},
    {"Spec-BVOH", 52.5f, 420, false},
    {"Spec-HIPS", 65.0f, 300, false},
    {"Spec-PA(CF,PET,PH*)", 85.0f, 540, false}, // 10
    {"Spec-PC(CF/FR)", 85.0f, 540, false},
    {"Spec-PC-ABS", 82.5f, 540, false},
    {"Spec-PET-CF", 75.0f, 480, false},
    {"Spec-PETG-CF", 70.0f, 480, false},
    {"Spec-PETG-HF", 65.0f, 360, false}, // 15
    {"Spec-PLA-CF", 55.0f, 360, false},
    {"Spec-PLA-HT", 55.0f, 360, false},
    {"Spec-PLA-WoodMetal", 45.0f, 300, false},
    {"Spec-POM", 70.0f, 300, false},
    {"Spec-PP", 55.0f, 300, false}, // 20
    {"Spec-PP-GF", 65.0f, 480, false},
    {"Spec-PPS(+CF)", 85.0f, 540, false},
    {"Spec-PVA", 50.0f, 480, false},
    {"Spec-PVDF-PPSU", 85.0f, 540, false},
    {"Spec-TPU 82A", 42.5f, 300, false}, // 25
    {"Spec-WOOD-Composite", 45.0f, 300, false},
};

static uint16_t g_remoteOutputsMask = 0;  // last STATUS mask (truth)
static uint16_t g_lastCommandMask = 0;    // last SET mask we sent
static uint16_t g_preWaitCommandMask = 0; // snapshot before WAIT

static uint32_t g_lastStatusRxMs = 0;
static uint32_t g_statusRxCount = 0;
static uint32_t g_commErrorCount = 0;

static constexpr uint32_t kCommAliveTimeoutMs = 1500; // tune as needed

static constexpr uint16_t kPresetCount =
    sizeof(kPresets) / sizeof(kPresets[0]);
static constexpr uint16_t OVEN_DEFAULT_PRESET_INDEX = 5; // PLA

// Initialization – called from setup()
void oven_init(void);

// Called in loop() to update timing, state machine, sensors, etc.
void oven_tick(void);

// Start/stop the drying process
void oven_start(void);
void oven_stop(void);
bool oven_is_running(void);

// // Profile access
// void oven_set_profile(const OvenProfile *profile);
// void oven_get_profile(OvenProfile *profileOut);

// Current runtime state for UI
void oven_get_runtime_state(OvenRuntimeState *stateOut);

// Manual commands triggered by icon buttons on the main screen
void oven_command_toggle_motor_manual(void);

void oven_fan230_toggle_manual(void);
void oven_lamp_toggle_manual(void);

// Pause/Resume (WAIT mode)
void oven_pause_wait(void);
bool oven_resume_from_wait(void);
bool oven_is_waiting(void);

uint16_t oven_get_preset_count(void);
void oven_get_preset_name(uint16_t index, char *out, size_t out_len);
void oven_select_preset(uint16_t index);
int oven_get_current_preset_index(void);
const FilamentPreset *oven_get_preset(uint16_t index);

// setters for runtime adjustments (non-persistent)
void oven_set_runtime_duration_minutes(uint16_t duration_min);
void oven_set_runtime_temp_target(uint16_t temp_c);

// setters for runtime actuator states (non-persistent)
// fan12V is never manually controlled
// door sensor is read-only
void oven_set_runtime_actuator_fan230(bool on);
void oven_set_runtime_actuator_fan230_slow(bool on);
void oven_set_runtime_actuator_heater(bool on);
void oven_set_runtime_actuator_motor(bool on);
void oven_set_runtime_actuator_lamp(bool on);

// Communication link (Host <-> Client over UART)
void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx);
void oven_comm_poll(void);

// END OF FILE