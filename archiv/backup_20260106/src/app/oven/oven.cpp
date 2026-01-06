#include "oven.h"
#include <Arduino.h>

// =============================================================================
// Internal static state (host-side)
// =============================================================================
//
// waiting: host-side WAIT mode flag (UI state machine)
// preWaitSnapshot: snapshot of UI-visible runtime fields to restore after WAIT
// hasPreWaitSnapshot: indicates snapshot validity
//
static bool waiting = false;
static OvenRuntimeState preWaitSnapshot = {};
static bool hasPreWaitSnapshot = false;

/**
 * currentProfile:
 * Host-side plan (duration + target temp + preset id).
 * The client remains authoritative for real actuator states and heating control.
 */
static OvenProfile currentProfile = {
    .durationMinutes = 60, // 1 hour default
    .targetTemperature = 45.0f,
    .filamentId = 0};

/**
 * runtimeState:
 * UI-facing snapshot.
 *
 * IMPORTANT (T6):
 * - Actuator booleans are remote truth and should be written only from telemetry.
 * - running/waiting are host-side state machine flags.
 * - countdown secondsRemaining is host-side (oven_tick()).
 */
static OvenRuntimeState runtimeState = {
    .durationMinutes = 60,
    .secondsRemaining = 60 * 60, // 1 hour in seconds

    .tempCurrent = 25.0f,
    .tempTarget = 40.0f,

    .tempToleranceC = 3.0f,
    .filamentId = 0,

    .fan12v_on = false,
    .fan230_on = false,
    .fan230_slow_on = false,
    .heater_on = false,
    .door_open = false,
    .motor_on = false,
    .lamp_on = false,

    .fan230_manual_allowed = true,
    .motor_manual_allowed = true,
    .lamp_manual_allowed = true,

    .running = false};

// =============================================================================
// HostComm integration (UART protocol, T6)
// =============================================================================
static HostComm *g_hostComm = nullptr;
static bool g_hasRealTelemetry = false;
static uint32_t g_lastStatusRequestMs = 0;

// =============================================================================
// Bitmask helpers
// =============================================================================

static inline uint16_t connector_u16(OVEN_CONNECTOR c) {
    return static_cast<uint16_t>(c);
}

/** Returns true if connector bit is set in mask. */
static inline bool mask_has(uint16_t mask, OVEN_CONNECTOR c) {
    return (mask & connector_u16(c)) != 0u;
}

/** Sets or clears connector bit in mask and returns new mask. */
static inline uint16_t mask_set(uint16_t mask, OVEN_CONNECTOR c, bool on) {
    if (on) {
        return static_cast<uint16_t>(mask | connector_u16(c));
    }
    return static_cast<uint16_t>(mask & ~connector_u16(c));
}

/**
 * Preserve "input-like" bits as reported by the client.
 *
 * In T6 we treat DOOR_ACTIVE as sensor telemetry and do not let the host
 * arbitrarily overwrite it. This helper ensures the outgoing SET mask keeps
 * the currently known door state from g_remoteOutputsMask.
 */
static inline uint16_t preserve_inputs(uint16_t mask) {
    const bool door = mask_has(g_remoteOutputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);
    mask = mask_set(mask, OVEN_CONNECTOR::DOOR_ACTIVE, door);
    return mask;
}

/**
 * Send outputs mask to client via HostComm, while keeping host bookkeeping.
 *
 * - Applies preserve_inputs()
 * - Updates g_lastCommandMask
 * - Issues HostComm->setOutputsMask()
 */
static inline void comm_send_mask(uint16_t newMask) {
    if (!g_hostComm) {
        return;
    }

    newMask = preserve_inputs(newMask);

    g_lastCommandMask = newMask;
    g_hostComm->setOutputsMask(newMask);
}

// =============================================================================
// Telemetry -> runtime mapping
// =============================================================================

/**
 * Apply ProtocolStatus (remote truth) into runtimeState (UI snapshot).
 *
 * NOTE:
 * - This is the ONLY place where actuator booleans should be updated in T6.
 * - Temperature scaling is currently provisional (tempRaw / 4.0f).
 */
static void apply_remote_status_to_runtime(const ProtocolStatus &st) {
    // TODO: confirm correct temperature scaling (tempRaw encoding)
    // Current assumption: tempRaw in quarter degrees => °C = tempRaw / 4.0
    runtimeState.tempCurrent = static_cast<float>(st.tempRaw) / 4.0f;

    // Output mapping using OVEN_CONNECTOR bit positions:
    runtimeState.fan12v_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_on = mask_has(st.outputsMask, OVEN_CONNECTOR::HEATER);
    runtimeState.lamp_on = mask_has(st.outputsMask, OVEN_CONNECTOR::LAMP);
    runtimeState.door_open = mask_has(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);

    // Mark that we have switched from "fake" to real telemetry
    g_hasRealTelemetry = true;

    // Cache remote truth mask for deriving future commands
    g_remoteOutputsMask = st.outputsMask;

    // Update diagnostics counters
    g_lastStatusRxMs = millis();
    g_statusRxCount++;
}

// =============================================================================
// Basic API
// =============================================================================

int oven_get_current_preset_index(void) {
    return runtimeState.filamentId;
}

const FilamentPreset *oven_get_preset(uint16_t index) {
    if (index >= kPresetCount) {
        return &kPresets[0]; // CUSTOM as fallback
    }
    return &kPresets[index];
}

/**
 * oven_init():
 * Initialize runtime defaults and select the default preset (factory behavior).
 * Must be called from setup().
 */
void oven_init(void) {
    runtimeState.tempCurrent = 0.0f;

    // Ensure runtimeState.presetName + currentProfile are consistent at boot
    oven_select_preset(OVEN_DEFAULT_PRESET_INDEX);

    OVEN_INFO("[OVEN] Init OK\n");
}

/**
 * oven_start():
 * Host-side START policy:
 * - running = true
 * - reset countdown from currentProfile
 * - send a SET mask to enforce START policy:
 *     HEATER ON
 *     FAN12V ON (cooling on powerboard)
 *     FAN230V_SLOW ON (cooling / airflow)
 *     FAN230V OFF (mutual exclusion with SLOW)
 *
 * NOTE: UI will reflect real output states only after telemetry arrives.
 */
void oven_start(void) {
    if (runtimeState.running) {
        return;
    }

    runtimeState.running = true;

    // Reset countdown only on START (if profile has sensible duration)
    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;

    uint16_t m = g_remoteOutputsMask;

    // START policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);

    // Mutual exclusion: FAST fan and SLOW fan should not be active together
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);

    // Door bit note:
    // In hardware the door line is used for safety features on the powerboard.
    // In T6 we treat DOOR_ACTIVE as input-like, so preserve_inputs() will keep
    // the sensor truth. If you *must* force a specific electrical behavior,
    // do it on the client side or revise preserve_inputs() intentionally.
    comm_send_mask(m);

    OVEN_INFO("[oven_start()]\n");
}

/**
 * oven_stop():
 * Host-side STOP policy:
 * - running = false
 * - send a SET mask to enforce STOP policy:
 *     HEATER OFF
 *     FAN12V OFF
 *     FAN230V OFF
 *     FAN230V_SLOW OFF
 *     MOTOR OFF
 */
void oven_stop(void) {
    if (!runtimeState.running) {
        return;
    }

    runtimeState.running = false;

    uint16_t m = g_remoteOutputsMask;

    // STOP policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);

    comm_send_mask(m);

    OVEN_INFO("[oven_stop]\n");
}

bool oven_is_running(void) {
    return runtimeState.running;
}

// =============================================================================
// Presets / Profile
// =============================================================================

/**
 * oven_select_preset():
 * Applies a factory preset to both currentProfile and runtimeState:
 * - duration, targetTemperature, filamentId, rotaryOn, presetName
 *
 * NOTE:
 * - This does NOT directly command actuators.
 * - For SILICA presets rotaryOn=true, the motor preference is stored in runtimeState.
 *   Actual motor output still follows policies and/or user manual toggles.
 */
void oven_select_preset(uint16_t index) {
    if (index >= kPresetCount) {
        return;
    }

    const FilamentPreset &p = kPresets[index];

    // Host plan
    currentProfile.durationMinutes = p.durationMin;
    currentProfile.targetTemperature = p.dryTempC;
    currentProfile.filamentId = index;

    // UI state
    runtimeState.durationMinutes = p.durationMin;
    runtimeState.secondsRemaining = p.durationMin * 60;
    runtimeState.tempTarget = p.dryTempC;
    runtimeState.filamentId = index;
    runtimeState.rotaryOn = p.rotaryOn;

    strncpy(runtimeState.presetName, p.name, sizeof(runtimeState.presetName) - 1);
    runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

    OVEN_INFO("[oven_select_preset] Preset selected: ", runtimeState.presetName);
}

uint16_t oven_get_preset_count(void) {
    return kPresetCount;
}

void oven_get_preset_name(uint16_t index, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return;
    }

    if (index >= kPresetCount) {
        out[0] = '\0';
        return;
    }

    std::strncpy(out, kPresets[index].name, out_len - 1);
    out[out_len - 1] = '\0';
    OVEN_DBG("[oven_get_preset_name]: ", out);
}

// =============================================================================
// Runtime access
// =============================================================================

void oven_get_runtime_state(OvenRuntimeState *out) {
    if (!out) {
        return;
    }
    *out = runtimeState;
}

// =============================================================================
// Timebase / countdown / diagnostics
// =============================================================================

/**
 * oven_tick():
 * Must be called frequently in loop(). Internally runs at 1 Hz.
 *
 * Responsibilities:
 * - Countdown timer (secondsRemaining) while running
 * - Communication diagnostics (commAlive, lastStatusAgeMs, counters)
 *
 * NOTE:
 * - This function does NOT do UART parsing; that is oven_comm_poll().
 */
void oven_tick(void) {
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < 1000) {
        return;
    }
    lastTick = now;

    // -------------------------------------------------------------------------
    // Fake temperature block intentionally disabled once real telemetry exists.
    // Kept as a placeholder for development without a client.
    // -------------------------------------------------------------------------
    if (!g_hasRealTelemetry) {
        // intentionally empty
    }

    // -------------------------------------------------------------------------
    // Countdown (host-side)
    // -------------------------------------------------------------------------
    if (runtimeState.running) {
        if (runtimeState.durationMinutes > 0) {
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            } else {
                oven_stop();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Communication diagnostics (host-side)
    // -------------------------------------------------------------------------
    now = millis();
    const uint32_t age = (g_lastStatusRxMs == 0) ? 0xFFFFFFFFu : (now - g_lastStatusRxMs);

    runtimeState.lastStatusAgeMs = age;
    runtimeState.commAlive = (g_lastStatusRxMs != 0) && (age <= kCommAliveTimeoutMs);
    runtimeState.statusRxCount = g_statusRxCount;
    runtimeState.commErrorCount = g_commErrorCount;

    // Future: optional auto-controls based on temperature could be added here.
}

// =============================================================================
// Manual Overrides (UI -> requests -> SET mask)
// =============================================================================
//
// Safety concept:
// - Only allow user toggles for certain outputs.
// - Never allow manual control for FAN12V, DOOR, HEATER.
// - For FAN230V we also enforce mutual exclusion with FAN230V_SLOW.
//
// NOTE: Changes become visible in UI only after next STATUS telemetry.
// =============================================================================

void oven_fan230_toggle_manual(void) {
    if (!runtimeState.fan230_manual_allowed) {
        return;
    }
    if (oven_is_running()) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::FAN230V);

    m = mask_set(m, OVEN_CONNECTOR::FAN230V, newState);
    if (newState) {
        // mutual exclusion
        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    }

    comm_send_mask(m);
    OVEN_INFO("[oven_fan230_toggle_manual] Fan230 request: ", (newState ? "ON" : "OFF"));
}

void oven_command_toggle_motor_manual(void) {
    if (!runtimeState.motor_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::SILICAT_MOTOR);

    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, newState);
    comm_send_mask(m);

    OVEN_INFO("[oven_command_toggle_motor_manual] Motor request: ", (newState ? "ON" : "OFF"));
}

void oven_lamp_toggle_manual(void) {
    if (!runtimeState.lamp_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::LAMP);

    m = mask_set(m, OVEN_CONNECTOR::LAMP, newState);
    comm_send_mask(m);

    OVEN_INFO("[oven_lamp_toggle_manual] Lamp request: ", (newState ? "ON" : "OFF"));
}

// =============================================================================
// WAIT / RESUME (host-side state machine + policy masks)
// =============================================================================

bool oven_is_waiting(void) {
    return waiting;
}

/**
 * oven_pause_wait():
 * Enter WAIT mode (pause) if currently running.
 *
 * Behavior:
 * - Snapshot UI-relevant runtime fields (preWaitSnapshot)
 * - Freeze countdown by setting running=false, waiting=true
 * - Store the last command mask as g_preWaitCommandMask for later RESUME
 * - Send WAIT policy mask:
 *     HEATER OFF
 *     MOTOR OFF
 *     FAN12V ON
 *     FAN230V_SLOW ON
 *     FAN230V OFF
 *     LAMP ON
 */
void oven_pause_wait(void) {
    if (!runtimeState.running || waiting) {
        OVEN_WARN("[oven_pause_wait] runtimeState.running=%d || waiting=%d\n",
                  runtimeState.running, waiting);
        return;
    }

    // Snapshot UI-related runtime fields
    preWaitSnapshot = runtimeState;
    hasPreWaitSnapshot = true;

    // FIX (important for RESUME): snapshot current command intent before changing it
    g_preWaitCommandMask = g_lastCommandMask;

    // Enter WAIT: stop countdown progression
    runtimeState.running = false;
    waiting = true;

    uint16_t m = g_remoteOutputsMask;

    // WAIT policy (safety-first)
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);

    // Cooling / visibility behavior during WAIT
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
    m = mask_set(m, OVEN_CONNECTOR::LAMP, true);

    comm_send_mask(m);

    OVEN_INFO("[oven_pause_wait] WAIT (paused)\n");
}

/**
 * oven_resume_from_wait():
 * Leave WAIT mode and continue countdown.
 *
 * Safety:
 * - Never resume if door is open (telemetry indicates DOOR_ACTIVE).
 *
 * Restore:
 * - Restores host/runtime UI fields (preset info, duration, tempTarget)
 * - Keeps secondsRemaining frozen value (continues where it stopped)
 * - Sends the pre-WAIT command mask again (g_preWaitCommandMask)
 */
bool oven_resume_from_wait(void) {
    if (!waiting) {
        OVEN_WARN("[oven_resume_from_wait] waiting=%d\n", waiting);
        return false;
    }

    // Safety rule: never resume when door open (telemetry)
    if (runtimeState.door_open) {
        OVEN_WARN("[oven_resume_from_wait] RESUME blocked: door open\n");
        return false;
    }

    // Restore UI-facing fields (do NOT restore actuator bools!)
    if (hasPreWaitSnapshot) {
        const uint32_t keepSeconds = runtimeState.secondsRemaining;

        runtimeState.durationMinutes = preWaitSnapshot.durationMinutes;
        runtimeState.tempTarget = preWaitSnapshot.tempTarget;
        runtimeState.filamentId = preWaitSnapshot.filamentId;
        runtimeState.rotaryOn = preWaitSnapshot.rotaryOn;

        std::strncpy(runtimeState.presetName, preWaitSnapshot.presetName,
                     sizeof(runtimeState.presetName) - 1);
        runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

        runtimeState.secondsRemaining = keepSeconds;
    }

    runtimeState.running = true;
    waiting = false;

    // Restore the pre-WAIT command mask
    comm_send_mask(g_preWaitCommandMask);

    OVEN_INFO("[oven_resume_from_wait] RESUME from WAIT\n");
    return true;
}

// =============================================================================
// Runtime adjustments (non-persistent) - host plan only
// =============================================================================

void oven_set_runtime_duration_minutes(uint16_t duration_min) {
    if (duration_min == 0) {
        return;
    }
    currentProfile.durationMinutes = duration_min;

    runtimeState.durationMinutes = duration_min;
    runtimeState.secondsRemaining = duration_min * 60;

    OVEN_INFO("[oven_set_runtime_duration_minutes] Runtime duration set to %d minutes\n", duration_min);
}

void oven_set_runtime_temp_target(uint16_t temp_c) {
    currentProfile.targetTemperature = static_cast<float>(temp_c);
    runtimeState.tempTarget = static_cast<float>(temp_c);
    OVEN_INFO("[oven_set_runtime_temp_target] Runtime target temperature set to %d °C\n", temp_c);
}

// =============================================================================
// Actuator setters (legacy / consider deprecating in strict T6 mode)
// =============================================================================
//
// In strict T6: actuator booleans represent remote truth and should only be set
// from telemetry (apply_remote_status_to_runtime). Keeping these functions can
// be confusing. They are left here for compatibility, but should not be used to
// "drive" outputs. Prefer policy masks / manual toggles instead.
// =============================================================================

void oven_set_runtime_actuator_fan230(bool on) {
    runtimeState.fan230_on = on;
    if (on) {
        runtimeState.fan230_slow_on = false;
    }
    OVEN_INFO("[oven_set_runtime_actuator_fan230] Runtime actuator fan230 set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_fan230_slow(bool on) {
    runtimeState.fan230_slow_on = on;
    if (on) {
        runtimeState.fan230_on = false;
    }
    OVEN_INFO("[oven_set_runtime_actuator_fan230_slow] Runtime actuator fan230_slow set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_heater(bool on) {
    runtimeState.heater_on = on;
    OVEN_INFO("[oven_set_runtime_actuator_heater] Runtime actuator heater set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_motor(bool on) {
    runtimeState.motor_on = on;
    OVEN_INFO("[oven_set_runtime_actuator_motor] Runtime actuator motor set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_lamp(bool on) {
    runtimeState.lamp_on = on;
    OVEN_INFO("[oven_set_runtime_actuator_lamp] Runtime actuator lamp set to %s\n", on ? "ON" : "OFF");
}

// =============================================================================
// Communication (HostComm) - initialization and polling
// =============================================================================

/**
 * oven_comm_init():
 * Binds HostComm to a HardwareSerial port and configures RX/TX pins + baudrate.
 * Must be called from setup() after oven_init() typically.
 */
void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx) {
    static HostComm comm(serial);
    g_hostComm = &comm;

    g_hostComm->begin(baudrate, rx, tx);

    g_hasRealTelemetry = false;
    g_lastStatusRequestMs = 0;

    OVEN_INFO("[oven_comm_init] HostComm init OK\n");
}

/**
 * oven_comm_poll():
 * Must be called frequently in loop().
 *
 * Responsibilities:
 * - Non-blocking UART RX parsing (HostComm::loop())
 * - Periodic STATUS request (kStatusPollIntervalMs)
 * - Apply latest STATUS to runtimeState
 * - Track protocol/parse errors
 */
void oven_comm_poll(void) {
    if (!g_hostComm) {
        return;
    }

    // Always read UART non-blocking
    g_hostComm->loop();

    // Periodically request STATUS
    const uint32_t now = millis();
    if (now - g_lastStatusRequestMs >= kStatusPollIntervalMs) {
        g_lastStatusRequestMs = now;
        g_hostComm->requestStatus();
    }

    // Apply new telemetry
    if (g_hostComm->hasNewStatus()) {
        apply_remote_status_to_runtime(g_hostComm->getRemoteStatus());
        g_hostComm->clearNewStatusFlag();
    }

    // FIX: handle comm error once (previously duplicated)
    if (g_hostComm->hasCommError()) {
        g_commErrorCount++;
        OVEN_WARN("[oven_comm_poll] HostComm parse/protocol error\n");
        g_hostComm->clearCommErrorFlag();
    }
}

// ---------------------------------------------------
// END OF FILE