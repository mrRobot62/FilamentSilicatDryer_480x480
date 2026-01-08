#pragma once

#include "HostComm.h"
#include "log_oven.h"
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

What the Host DOES:
- Maintains *intent/policy* (START/STOP/WAIT/RESUME policies).
- Sends SET masks derived from the most recent remote STATUS mask.
- Runs the countdown timer only in oven_tick() (1 Hz).
- Provides diagnostics (commAlive, lastStatusAgeMs, counters).

ASCII overview:

   +--------------------+                     +----------------------+
   |      UI (LVGL)     |   reads state only  |  oven_get_runtime...  |
   | (screen_main etc.) |<--------------------|  OvenRuntimeState     |
   +---------+----------+                     +----------+-----------+
             |                                        |
             | user actions                           | updated from telemetry
             v                                        v
   +--------------------+   policies/commands  +----------------------+
   |       oven.*       |--------------------->|      HostComm        |
   | (host logic +      |   UART ASCII (CRLF)  | (ProtocolCodec etc.) |
   |  countdown + diag) |<---------------------|                      |
   +--------------------+    STATUS telemetry  +----------+-----------+
                                                          |
                                                          | UART
                                                          v
                                               +----------------------+
                                               | Client/Powerboard    |
                                               | (authoritative I/O)  |
                                               +----------------------+

Practical consequences:
- START/STOP/WAIT send *policy masks*; UI changes only after STATUS confirms.
- Manual toggles (fan230/motor/lamp) are requests (SET masks), not direct writes.
- Door is treated as an input-like bit: preserve it as reported by the client.

================================================================================
*/

enum class OVEN_CONNECTOR : uint16_t {
    FAN12V = 1u << 0,        // bit 0
    FAN230V = 1u << 1,       // bit 1
    FAN230V_SLOW = 1u << 2,  // bit 2
    SILICAT_MOTOR = 1u << 3, // bit 3
    HEATER = 1u << 4,        // bit 4
    LAMP = 1u << 5,          // bit 5
    DOOR_ACTIVE = 1u << 6    // bit 6 (input-like sensor on client side)
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
constexpr uint32_t kStatusPollIntervalMs = 2000; // request STATUS every 2 seconds

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

/**
 * OvenProfile:
 * Host-side configuration that represents the active drying setup:
 * - durationMinutes: total duration
 * - targetTemperature: target temperature in °C
 * - filamentId: index into kPresets (0 = CUSTOM)
 *
 * Note: This is a "host plan". The actual heating behavior is handled by the client.
 */
typedef struct
{
    uint32_t durationMinutes; // in minutes
    float targetTemperature;  // in Celsius
    int filamentId;           // FilamentPreset index
} OvenProfile;

/**
 * OvenRuntimeState:
 * The single UI-facing state snapshot.
 *
 * IMPORTANT:
 * - Actuator booleans (fan/heater/motor/lamp) should represent the *remote truth*
 *   and must be derived from client telemetry (STATUS).
 * - Fields like running/waiting are host-side state machine flags.
 * - Countdown secondsRemaining is host-side (oven_tick()).
 */
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

    // Remote truth (from client STATUS)
    bool fan12v_on;
    bool fan230_on;
    bool fan230_slow_on;
    bool heater_on;
    bool door_open;
    bool motor_on;
    bool lamp_on;

    // Safety gates for UI manual toggles
    bool fan230_manual_allowed;
    bool motor_manual_allowed;
    bool lamp_manual_allowed;

    // Communication diagnostics (host-side)
    bool commAlive;
    uint32_t lastStatusAgeMs;
    uint32_t statusRxCount;
    uint32_t commErrorCount;

    // Host-side mode (not remote truth)
    OvenMode mode;

    // Backward-compat flag (temporary; will be removed once UI switches to mode)
    bool running;

    // T7: POST runtime state
    PostRuntime post;

} OvenRuntimeState;

// ----------------------------------------------------------------------------
// Preset list (immutable factory presets)
// ----------------------------------------------------------------------------

static constexpr FilamentPreset kPresets[] = {
    // Name, Temp(C), Dur(min), Rotary, POST{active, seconds, fanMode}
    {"CUSTOM", 0.0f, 0, false, {false, 0, PostFanMode::SLOW}},
    {"SILICA", 105.0f, 90, true, {true, 300, PostFanMode::FAST}}, // example: 5 min cooling
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

// static constexpr FilamentPreset kPresets[] = {
//     // Name, Temp(C), Dur(min), Rotary
//     {"CUSTOM", 0.0f, 0, false},
//     {"SILICA", 105.0f, 90, true},
//     {"ABS", 80.0f, 300, false},
//     {"ASA", 82.5f, 300, false},
//     {"PETG", 62.5f, 360, false},
//     {"PLA", 47.5f, 300, false}, // 5
//     {"TPU", 45.0f, 300, false},
//     {"Spec-ASA-CF", 85.0f, 540, false},
//     {"Spec-BVOH", 52.5f, 420, false},
//     {"Spec-HIPS", 65.0f, 300, false},
//     {"Spec-PA(CF,PET,PH*)", 85.0f, 540, false}, // 10
//     {"Spec-PC(CF/FR)", 85.0f, 540, false},
//     {"Spec-PC-ABS", 82.5f, 540, false},
//     {"Spec-PET-CF", 75.0f, 480, false},
//     {"Spec-PETG-CF", 70.0f, 480, false},
//     {"Spec-PETG-HF", 65.0f, 360, false}, // 15
//     {"Spec-PLA-CF", 55.0f, 360, false},
//     {"Spec-PLA-HT", 55.0f, 360, false},
//     {"Spec-PLA-WoodMetal", 45.0f, 300, false},
//     {"Spec-POM", 70.0f, 300, false},
//     {"Spec-PP", 55.0f, 300, false}, // 20
//     {"Spec-PP-GF", 65.0f, 480, false},
//     {"Spec-PPS(+CF)", 85.0f, 540, false},
//     {"Spec-PVA", 50.0f, 480, false},
//     {"Spec-PVDF-PPSU", 85.0f, 540, false},
//     {"Spec-TPU 82A", 42.5f, 300, false}, // 25
//     {"Spec-WOOD-Composite", 45.0f, 300, false},
// };

// ----------------------------------------------------------------------------
// Remote/command mask tracking (host-side)
// ----------------------------------------------------------------------------
//
// g_remoteOutputsMask: last outputsMask received from STATUS (remote truth)
// g_lastCommandMask:  last SET mask sent by host (for logging/debug)
// g_preWaitCommandMask: snapshot of "pre-WAIT" command mask for RESUME restoration
//
static uint16_t g_remoteOutputsMask = 0;  // last STATUS mask (truth)
static uint16_t g_lastCommandMask = 0;    // last SET mask we sent
static uint16_t g_preWaitCommandMask = 0; // snapshot before WAIT

// Communication counters/timestamps
static uint32_t g_lastStatusRxMs = 0;
static uint32_t g_statusRxCount = 0;
static uint32_t g_commErrorCount = 0;

// Alive heuristic (host-side)
static constexpr uint32_t kCommAliveTimeoutMs = 1500; // tune as needed

static constexpr uint16_t kPresetCount =
    sizeof(kPresets) / sizeof(kPresets[0]);
static constexpr uint16_t OVEN_DEFAULT_PRESET_INDEX = 5; // PLA

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

// Initialization – called from setup()
void oven_init(void);

// Called in loop() to update timing, state machine, sensors, etc.
void oven_tick(void);

// Start/stop the drying process (host policy -> SET mask)
void oven_start(void);
void oven_stop(void);
bool oven_is_running(void);

// Current runtime state for UI (single read-only snapshot)
void oven_get_runtime_state(OvenRuntimeState *stateOut);

// Manual commands triggered by icon buttons on the main screen
void oven_command_toggle_motor_manual(void);
void oven_fan230_toggle_manual(void);
void oven_lamp_toggle_manual(void);

// Pause/Resume (WAIT mode)
void oven_pause_wait(void);
bool oven_resume_from_wait(void);
bool oven_is_waiting(void);

// Preset helpers
uint16_t oven_get_preset_count(void);
void oven_get_preset_name(uint16_t index, char *out, size_t out_len);
void oven_select_preset(uint16_t index);
int oven_get_current_preset_index(void);
const FilamentPreset *oven_get_preset(uint16_t index);

// Setters for runtime adjustments (non-persistent):
// These affect the host-side profile/countdown targets only.
// (Remote control policies still go via SET masks.)
void oven_set_runtime_duration_minutes(uint16_t duration_min);
void oven_set_runtime_temp_target(uint16_t temp_c);

// NOTE ABOUT ACTUATOR "SETTERS":
// In T6, actuator booleans are remote truth and should be updated only from STATUS.
// If you keep these setters, treat them as *host-local placeholders* or deprecate them.
// The preferred way is to implement actuator requests via mask policies (like manual toggles).
void oven_set_runtime_actuator_fan230(bool on);
void oven_set_runtime_actuator_fan230_slow(bool on);
void oven_set_runtime_actuator_heater(bool on);
void oven_set_runtime_actuator_motor(bool on);
void oven_set_runtime_actuator_lamp(bool on);

// Communication link (Host <-> Client over UART)
void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx);
void oven_comm_poll(void);

// END OF FILE