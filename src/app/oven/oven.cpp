#include "oven_utils.h" // implicde #include "oven.h"

#include <Arduino.h>

// =============================================================================
// Internal static state (host-side)
// =============================================================================
//
// waiting: host-side WAIT mode flag (UI state machine)
// preWaitSnapshot: snapshot of UI-visible runtime fields to restore after WAIT
// hasPreWaitSnapshot: indicates snapshot validity
//

static OvenRuntimeState preWaitSnapshot = {};
static bool hasPreWaitSnapshot = false;

// NOTE (T7): waiting is now represented by runtimeState.mode.
// Keep this legacy flag only for transitional code paths if needed.
static bool waiting = false;
// --- T7/AP3.1: Fail-safe on LinkSync rising edge ---
static bool g_prevLinkSynced = false;
// --- T7/AP3.2: Alive timeout fail-safe ---
static uint32_t g_lastRxGoodMs = 0;
static bool g_aliveTimeoutTripped = false;

static uint32_t g_lastRxMs = 0;     // last time we received a valid STATUS/ACK/PONG
static bool g_safeStopSent = false; // ensure SAFE STOP only once per outage

static uint32_t g_lastPingMs = 0;
static bool g_sentSafeStopOnThisSync = false;

// Choose a conservative timeout (ms)
// Tunables
static constexpr uint32_t kAliveTimeoutMs = 1500;        // e.g. 1.5s
static constexpr uint32_t kPingIntervalUnsyncedMs = 250; // e.g. 4Hz while unsynced
static constexpr uint32_t kPingIntervalSyncedMs = 1000;  // keep-alive while synced (prevents UI flicker)

// ----------------------------------------------------------------------------
// Remote/command mask tracking (host-side)
// ----------------------------------------------------------------------------
static uint16_t g_remoteOutputsMask = 0;  // last STATUS mask (truth)
static uint16_t g_lastCommandMask = 0;    // last SET mask we sent
static uint16_t g_preWaitCommandMask = 0; // snapshot before WAIT

// Communication counters/timestamps
static uint32_t g_lastStatusRxMs = 0;
static uint32_t g_statusRxCount = 0;
static uint32_t g_commErrorCount = 0;

// Alive heuristic (host-side)
static constexpr uint32_t kCommAliveTimeoutMs = 1500; // tune as needed

static bool g_hostOvertempActive = false;

/**
 * currentProfile:
 * Host-side plan (duration + target temp + preset id).
 * The client remains authoritative for real actuator states and heating control.
 */
static OvenProfile currentProfile = {
    .durationMinutes = 60, // 1 hour default
    .targetTemperature = 45.0f,
    .filamentId = 0};

// T7: Cache the selected preset's POST plan so it cannot be accidentally
// affected by later runtime edits or UI side-effects.
static PostConfig g_currentPostPlan = {false, 0, PostFanMode::FAST};

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

    .running = false,

    // new in T7
    .post = {false, 0, 0}

};

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

static void force_local_safe_stop_due_to_comm(const char *reason) {
    (void)reason;

    // Hard STOP for UI / state machine
    runtimeState.mode = OvenMode::STOPPED;
    runtimeState.running = false; // legacy UI flag

    // Legacy flag still exists in oven.cpp (optional, but consistent)
    waiting = false;

    // Clear POST runtime
    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    // Local actuator view -> all off (UI shows OFF)
    runtimeState.fan12v_on = false;
    runtimeState.fan230_on = false;
    runtimeState.fan230_slow_on = false;
    runtimeState.motor_on = false;
    runtimeState.heater_on = false;
    runtimeState.lamp_on = false;

    // Comms flags (UI)
    runtimeState.commAlive = false;
    runtimeState.linkSynced = false;
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
    // Only start from STOPPED (or allow restart from POST/WAIT later if needed)
    if (runtimeState.mode == OvenMode::RUNNING) {
        return;
    }
    if (runtimeState.mode == OvenMode::WAITING) {
        return; // user must RESUME, not START
    }

    runtimeState.mode = OvenMode::RUNNING;
    runtimeState.running = true; // backward-compat for current UI

    // Reset countdown only on START (if profile has sensible duration)
    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;

    // Reset POST runtime
    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    uint16_t m = g_remoteOutputsMask;

    // START policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);

    // Mutual exclusion: FAST fan and SLOW fan should not be active together
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);

    // ------------------
    // Door bit note:
    // ------------------
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
    // STOP is allowed from any active mode
    if (runtimeState.mode == OvenMode::STOPPED) {
        return;
    }

    runtimeState.mode = OvenMode::STOPPED;
    runtimeState.running = false; // backward-compat
    waiting = false;

    // Clear POST runtime
    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    uint16_t m = g_remoteOutputsMask;

    // STOP policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);
    m = mask_set(m, OVEN_CONNECTOR::LAMP, false);

    comm_send_mask(m);

    OVEN_INFO("[oven_stop]\n");
}

bool oven_is_running(void) {
    // T6
    // return runtimeState.running;

    // new in T7
    return runtimeState.mode == OvenMode::RUNNING;
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

    // T7: Cache POST plan from preset
    g_currentPostPlan = p.post;

    strncpy(runtimeState.presetName, p.name, sizeof(runtimeState.presetName) - 1);
    runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

    OVEN_INFO("[oven_select_preset] Preset selected: %s\n", runtimeState.presetName);
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
    OVEN_DBG("[oven_get_preset_name]: %s\n", out);
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
    // Countdown (host-side) T7
    // Mode-based time progression (host-side)
    // -------------------------------------------------------------------------
    if (runtimeState.mode == OvenMode::RUNNING) {
        if (runtimeState.durationMinutes > 0) {
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            } else {
                // Transition to POST if configured; otherwise STOP.
                if (g_currentPostPlan.active && g_currentPostPlan.seconds > 0) {
                    runtimeState.mode = OvenMode::POST;
                    runtimeState.running = false; // UI legacy

                    runtimeState.post.active = true;
                    runtimeState.post.secondsRemaining = g_currentPostPlan.seconds;
                    runtimeState.post.stepIndex = 0;

                    // T7: Show POST countdown on the main dial (UI uses secondsRemaining)
                    runtimeState.secondsRemaining = g_currentPostPlan.seconds;
                    runtimeState.durationMinutes = (g_currentPostPlan.seconds + 59u) / 60u;

                    runtimeState.post.secondsRemaining = g_currentPostPlan.seconds;
                    runtimeState.secondsRemaining = g_currentPostPlan.seconds;

                    // Apply POST policy (Cooling step 0)
                    uint16_t m = g_remoteOutputsMask;
                    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
                    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);
                    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
                    m = mask_set(m, OVEN_CONNECTOR::LAMP, true);

                    if (g_currentPostPlan.fanMode == PostFanMode::SLOW) {
                        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
                        m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
                    } else {
                        m = mask_set(m, OVEN_CONNECTOR::FAN230V, true);
                        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
                    }

                    comm_send_mask(m);

                    OVEN_INFO("[oven_tick] RUN->POST (step=%u, seconds=%u, fan=%s)\n",
                              (unsigned)runtimeState.post.stepIndex,
                              (unsigned)runtimeState.post.secondsRemaining,
                              (g_currentPostPlan.fanMode == PostFanMode::SLOW) ? "SLOW" : "FAST");
                } else {
                    OVEN_INFO("[oven_tick] RUN finished -> STOP (no POST configured)\n");
                    oven_stop();
                }
            }
        }
    } else if (runtimeState.mode == OvenMode::POST) {
        if (runtimeState.post.active && runtimeState.post.secondsRemaining > 0) {
            runtimeState.post.secondsRemaining--;

            // Keep the main countdown in sync for the UI dial
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            }
        } else {
            // POST finished -> STOP
            oven_stop();
            OVEN_INFO("[oven_tick] POST finished -> STOP\n");
        }
    }

    // -------------------------------------------------------------------------
    // Communication diagnostics (host-side)
    // -------------------------------------------------------------------------

    // NOTE (T7/AP4.2): commAlive/linkSynced are maintained in oven_comm_poll()
    // from HostComm RX timestamps + handshake state. Do NOT overwrite them here,
    // otherwise the UI may flicker.

    runtimeState.statusRxCount = g_statusRxCount;
    runtimeState.commErrorCount = g_commErrorCount;

    // Future: optional auto-controls based on temperature could be added here.

    // -------------------------------------------------------------------------
    // HOST-side overtemperature control (soft safety, non-fatal)
    // -------------------------------------------------------------------------
    if (runtimeState.mode == OvenMode::RUNNING) {
        const float cur = runtimeState.tempCurrent;
        const float tgt = runtimeState.tempTarget;
        const float tol = HOST_TEMP_TOLERANCE_C;
        const float hi = tgt + tol;
        const float lo = tgt - tol;
        
        // Trip: enter overtemp lock
        if (!g_hostOvertempActive && (cur >= hi)) {
            g_hostOvertempActive = true;
            OVEN_WARN("[SAFETY] OVER-TEMP -> lock active (cur=%.1f >= hi=%.1f)\n", cur, hi);

            uint16_t m = g_remoteOutputsMask;

            // Heater OFF
            m = mask_set(m, OVEN_CONNECTOR::HEATER, false);

            // T10.1.39: fast cooldown airflow while overtemp-locked
            m = mask_set(m, OVEN_CONNECTOR::FAN230V, true);
            m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);

            comm_send_mask(m);

            OVEN_INFO("[HOST-OT] lock -> HEATER OFF, FAN230=ON, FAN230_SLOW=OFF (cur=%.1f, tgt=%.1f, tol=%.1f)\n",
                      cur, tgt, tol);
        } else if (g_hostOvertempActive && (cur <= lo)) {
            g_hostOvertempActive = false;
            OVEN_INFO("[SAFETY] OVER-TEMP recovered (cur=%.1f <= lo=%.1f)\n", cur, lo);

            uint16_t m = g_remoteOutputsMask;

            // T10.1.39: restore normal airflow when leaving overtemp lock
            m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
            m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);

            // (Optional) Heater is not forced ON here; heater hysteresis will decide below.
            // If you want it *immediately* when recovery hits AND cur<=lo => heaterWanted true anyway.
            comm_send_mask(m);

            OVEN_INFO("[HOST-OT] recovered -> FAN230=OFF, FAN230_SLOW=ON (cur=%.1f)\n", cur);
        }
        // // --- Trip condition ---
        // if (!g_hostOvertempActive) {
        //     if (cur >= (tgt + tol)) {
        //         g_hostOvertempActive = true;

        //         uint16_t m = g_remoteOutputsMask;
        //         m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
        //         comm_send_mask(m);
        //         OVEN_WARN("[HOST-OT] OVER-TEMP: %.1f >= %.1f (target %.1f + tol %.1f) -> HEATER OFF\n",
        //                   cur, tgt + tol, tgt, tol);
        //     }
        // }
        // // --- Recovery (hysteresis) ---
        // else {
        //     // hysteresis: re-enable slightly below target
        //     if (cur <= (tgt - tol)) {
        //         g_hostOvertempActive = false;

        //         uint16_t m = g_remoteOutputsMask;
        //         m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
        //         comm_send_mask(m);

        //         OVEN_INFO("[HOST-OT] temp %.1f back in range -> HEATER ON\n", cur);
        //     }
        // }
    } else {
        // Leaving RUNNING clears host overtemp latch
        g_hostOvertempActive = false;
    }
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
    OVEN_INFO("[oven_fan230_toggle_manual] Fan230 request: %s\n", (newState ? "ON" : "OFF"));
}

void oven_command_toggle_motor_manual(void) {
    if (!runtimeState.motor_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::SILICAT_MOTOR);

    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, newState);
    comm_send_mask(m);

    OVEN_INFO("[oven_command_toggle_motor_manual] Motor request: %s\n", (newState ? "ON" : "OFF"));
}

void oven_lamp_toggle_manual(void) {
    if (!runtimeState.lamp_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::LAMP);

    m = mask_set(m, OVEN_CONNECTOR::LAMP, newState);
    comm_send_mask(m);

    OVEN_INFO("[oven_lamp_toggle_manual] Lamp request: %s\n", (newState ? "ON" : "OFF"));
}

void oven_dbg_hw_toggle_by_index(int idx) {
    if (!g_hostComm) {
        return;
    }

    // Optional: only allow when link is synced
    if (!g_hostComm->linkSynced()) {
        OVEN_WARN("[oven_dbg_hw_toggle_by_index] ignored: not synced\n");
        return;
    }

    // Map 0..6 to connector bits
    OVEN_CONNECTOR c;
    switch (idx) {
    case 0:
        c = OVEN_CONNECTOR::FAN12V;
        break;
    case 1:
        c = OVEN_CONNECTOR::FAN230V;
        break;
    case 2:
        c = OVEN_CONNECTOR::FAN230V_SLOW;
        break;
    case 3:
        c = OVEN_CONNECTOR::HEATER;
        break;
    case 4:
        // Door is input-like: ignore in debug toggles
        OVEN_WARN("[oven_dbg_hw_toggle_by_index] DOOR ignored\n");
        return;
    case 5:
        c = OVEN_CONNECTOR::SILICAT_MOTOR;
        break;
    case 6:
        c = OVEN_CONNECTOR::LAMP;
        break;
    default:
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool cur = mask_has(m, c);
    const bool next = !cur;

    m = mask_set(m, c, next);

    // Mutual exclusion: FAN230V <-> FAN230V_SLOW
    if (c == OVEN_CONNECTOR::FAN230V && next) {
        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    }
    if (c == OVEN_CONNECTOR::FAN230V_SLOW && next) {
        m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    }

    // Send
    comm_send_mask(m);

    // Optional: ask status immediately to speed up UI feedback if ACK isn't enough
    g_hostComm->requestStatus();

    OVEN_INFO("[oven_dbg_hw_toggle_by_index] idx=%d -> %s (mask=%s)\n",
              idx, next ? "ON" : "OFF", oven_outputs_mask_to_str(m));
}

void oven_force_outputs_off(void) {
    // Best-effort: clear all outputs on client
    comm_send_mask(0x0000);

    // Keep local shadow consistent
    g_remoteOutputsMask = 0;

    OVEN_WARN("[oven] force outputs OFF\n");
}

// =============================================================================
// WAIT / RESUME (host-side state machine + policy masks)
// =============================================================================

bool oven_is_waiting(void) {
    // return waiting;
    //  NEW T7
    return runtimeState.mode == OvenMode::WAITING;
}

bool oven_is_alive(void) {
    if (g_hostComm != nullptr && g_hostComm->linkSynced()) {
        return true;
    }
    return false;
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
    // if (!runtimeState.running || waiting) {
    //  if condition new T7
    if (runtimeState.mode != OvenMode::RUNNING || runtimeState.mode == OvenMode::WAITING) {
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
    runtimeState.mode = OvenMode::WAITING;
    runtimeState.running = false; // backward-compat

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
    // if condition new in T7
    if (runtimeState.mode != OvenMode::WAITING) {
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

    runtimeState.mode = OvenMode::RUNNING;
    runtimeState.running = true; // backward-compat
    waiting = false;             // legacy

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

    // --- AP3.2 init ---
    g_prevLinkSynced = false;
    g_lastRxMs = millis(); // start "fresh"
    g_safeStopSent = false;
    g_lastPingMs = 0;
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

    uint32_t now = millis();
    // 1) Always read UART non-blocking
    g_hostComm->loop();
    now = millis();

    // 2) Mirror comm diagnostics into runtime (UI reads only runtimeState)
    runtimeState.linkSynced = g_hostComm->linkSynced();

    const uint32_t lastRxAny = g_hostComm->lastRxAnyMs();
    const uint32_t lastStatus = g_hostComm->lastStatusMs();

    runtimeState.lastRxAnyAgeMs = (lastRxAny > 0) ? (now - lastRxAny) : 0xFFFFFFFFu;
    runtimeState.lastStatusAgeMs = (lastStatus > 0) ? (now - lastStatus) : 0xFFFFFFFFu;

    const bool alive = (lastRxAny > 0) && ((now - lastRxAny) <= kAliveTimeoutMs);
    runtimeState.commAlive = alive;

    // OVEN_DBG("----> lastRxAny=%lu, lastStatus=%lu, lastRxAnyAge=%lu, lastStatusAge=%lu, alive=%d, linkSynced=%d\n",
    //          (unsigned long)lastRxAny,
    //          (unsigned long)lastStatus,
    //          (unsigned long)runtimeState.lastRxAnyAgeMs,
    //          (unsigned long)runtimeState.lastStatusAgeMs,
    //          alive ? 1 : 0,
    //          runtimeState.linkSynced ? 1 : 0);

    // 3) Handshake driver: if not synced -> keep sending PING
    // 3) Handshake + keep-alive: send PING while unsynced (fast) and also while synced (slow)
    const uint32_t pingInterval = runtimeState.linkSynced ? kPingIntervalSyncedMs : kPingIntervalUnsyncedMs;
    if (now - g_lastPingMs >= pingInterval) {
        g_lastPingMs = now;
        g_hostComm->sendPing();
    }

    // 4) Rising edge: link became stable -> send SAFE STOP ONCE per sync-session
    if (!runtimeState.linkSynced) {
        g_sentSafeStopOnThisSync = false; // re-arm for next successful sync
    }

    if (runtimeState.linkSynced && !g_sentSafeStopOnThisSync) {
        OVEN_WARN("[oven_comm_poll] LinkSynced -> SAFE STOP once (SET 0x0000)\n");
        comm_send_mask(0x0000);
        g_sentSafeStopOnThisSync = true;
    }

    // 5) Poll STATUS periodically (optional: only when synced)
    if (now - g_lastStatusRequestMs >= kStatusPollIntervalMs) {
        g_lastStatusRequestMs = now;
        g_hostComm->requestStatus();
    }

    // 6) Apply new telemetry
    if (g_hostComm->hasNewStatus()) {
        apply_remote_status_to_runtime(g_hostComm->getRemoteStatus());
        g_hostComm->clearNewStatusFlag();
    }

    // -------------------------------------------------------------------------
    // T10.1.38 - Host heater control (RUNNING only)
    // - Hysteresis around target: ON below (target - tol), OFF above (target + tol)
    // - Overtemp lockout uses same band, clears only when below (target - tol)
    // - Door open always forces HEATER OFF (best-effort; client is authoritative)
    // -------------------------------------------------------------------------
    if (runtimeState.mode == OvenMode::RUNNING) {
        // ---- T10.1.38: reduce HEATER log spam (log only on state change) ----
        static bool g_lastHeaterRequest = false;
        static bool g_lastOvertempLock = false;

        const float cur = runtimeState.tempCurrent;
        const float tgt = runtimeState.tempTarget;
        const float tol = runtimeState.tempToleranceC;

        const float hi = tgt + tol;
        const float lo = tgt - tol;

        // 1) Door safety (host best-effort)
        // 1) Door safety (host best-effort)
        if (runtimeState.door_open) {
            if (mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER)) {
                uint16_t m = g_lastCommandMask;
                m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
                comm_send_mask(m);
                OVEN_WARN("[SAFETY] DOOR OPEN -> HEATER OFF (host)\n");
            }
        } else {
            // 2) Overtemp lock (latched until temp is back below (tgt - tol))
            if (!g_hostOvertempActive && (cur >= hi)) {
                g_hostOvertempActive = true;
                OVEN_WARN("[SAFETY] OVER-TEMP -> lock active (cur=%.1f >= hi=%.1f)\n", cur, hi);
            } else if (g_hostOvertempActive && (cur <= lo)) {
                g_hostOvertempActive = false;
                OVEN_INFO("[SAFETY] OVER-TEMP recovered (cur=%.1f <= lo=%.1f)\n", cur, lo);
            }

            // 3) Apply heater decision (only affect HEATER bit)
            //
            // IMPORTANT:
            // - g_remoteOutputsMask is telemetry truth and may lag until next STATUS.
            // - Use g_lastCommandMask as host "intent" to avoid spamming SET/LOG.
            const bool heaterWanted = (!g_hostOvertempActive) && (cur <= lo);

            // "Current" from host intent (last command we sent), not from telemetry.
            const bool heaterIsOn = mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER);

            // OFF condition: either lock active or above hi (normal hysteresis top)
            const bool mustForceOff = g_hostOvertempActive || (cur >= hi);

            if (mustForceOff && heaterIsOn) {
                uint16_t m = g_lastCommandMask; // <- use intent base
                m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
                comm_send_mask(m);

                OVEN_INFO("[HEATER] OFF (cur=%.1f, tgt=%.1f, tol=%.1f, lock=%d)\n",
                          cur, tgt, tol, g_hostOvertempActive ? 1 : 0);
            } else if (heaterWanted && !heaterIsOn) {
                uint16_t m = g_lastCommandMask; // <- use intent base
                m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
                comm_send_mask(m);

                OVEN_INFO("[HEATER] ON  (cur=%.1f, tgt=%.1f, tol=%.1f)\n", cur, tgt, tol);
            }
            // else: hold state
        }
    } else {
        // Not RUNNING: ensure lock is cleared so next RUN starts clean
        g_hostOvertempActive = false;
    }

    // 7) ACK-based outputs update (fast UI feedback)
    if (g_hostComm->lastSetAcked() || g_hostComm->lastUpdAcked() || g_hostComm->lastTogAcked()) {
        const ProtocolStatus &st = g_hostComm->getRemoteStatus();

        runtimeState.fan12v_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN12V);
        runtimeState.fan230_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V);
        runtimeState.fan230_slow_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
        runtimeState.motor_on = mask_has(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
        runtimeState.heater_on = mask_has(st.outputsMask, OVEN_CONNECTOR::HEATER);
        runtimeState.lamp_on = mask_has(st.outputsMask, OVEN_CONNECTOR::LAMP);
        runtimeState.door_open = mask_has(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);

        OVEN_INFO("[DOOR] outputsMask=0x%04X door_open=%d commAlive=%d linkSynced=%d\n",
                  st.outputsMask,
                  runtimeState.door_open ? 1 : 0,
                  runtimeState.commAlive ? 1 : 0,
                  runtimeState.linkSynced ? 1 : 0);

        g_remoteOutputsMask = st.outputsMask;

        if (g_hostComm->lastSetAcked()) {
            g_hostComm->clearLastSetAckFlag();
        }
        if (g_hostComm->lastUpdAcked()) {
            g_hostComm->clearLastUpdAckFlag();
        }
        if (g_hostComm->lastTogAcked()) {
            g_hostComm->clearLastTogAckFlag();
        }
    }

    // 8) Alive timeout -> SAFE STOP once.
    // IMPORTANT: We keep the handshake state stable. LinkSynced reflects the PONG-streak handshake.
    // commAlive reflects freshness of RX traffic.
    if ((lastRxAny > 0) && !alive) {
        if (!g_safeStopSent) {
            OVEN_WARN("[oven_comm_poll] Alive timeout (%lums) -> SAFE STOP (SET 0x0000)\n",
                      (unsigned long)(now - lastRxAny));

            // 1) Command the client to a safe OFF state (best-effort).
            comm_send_mask(0x0000);
            g_safeStopSent = true;

            // 2) Immediately force local UI/state-machine into safe STOP.
            //    This prevents the UI from showing stale colored actuator icons.
            force_local_safe_stop_due_to_comm("alive-timeout");
            g_remoteOutputsMask = 0;
        }

        // Force re-sync so we re-handshake when client returns
        g_hostComm->clearLinkSync();
        g_sentSafeStopOnThisSync = false;
    } else {
        // Link alive again -> allow future timeout reactions
        g_safeStopSent = false;
    }

    // 9) Comm error flag
    if (g_hostComm->hasCommError()) {
        g_commErrorCount++;
        OVEN_WARN("[oven_comm_poll] HostComm parse/protocol error\n");
        g_hostComm->clearCommErrorFlag();
    }

    // 10) OneTime AliveFlag
    static bool prevAlive = false;

    if (!prevAlive && runtimeState.commAlive) {
        OVEN_INFO("[oven_comm_poll] Link alive & synced\n");
    }

    prevAlive = runtimeState.commAlive;
}

// ---------------------------------------------------
// END OF FILE