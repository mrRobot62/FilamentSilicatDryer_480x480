/**
 * =============================================================================
 * oven.cpp — Host-side oven logic (T11 thermal model rework)
 * =============================================================================
 *
 * This file is intentionally heavily documented.
 * It is the single place where host policy and the T11 thermal model are applied.
 *
 * Architecture recap (T6+):
 * - Host (ESP32-S3): UI + policy + countdown + thermal model.
 * - Client (ESP32-WROOM): authoritative IO (outputs + sensors) and telemetry.
 * - UART protocol (HostComm): line-based ASCII frames with CRLF.
 *
 * "Single Source of Truth":
 * - UI renders ONLY from OvenRuntimeState.
 * - Actuator booleans in OvenRuntimeState are updated ONLY from client telemetry
 *   (C;STATUS). Host commands are intents; telemetry is truth.
 *
 * -----------------------------------------------------------------------------
 * T11 Thermal Model (Variant B: PT1 + bias)
 * -----------------------------------------------------------------------------
 * Sensor reality:
 * - The NTC is physically close to the heater and sees a hot spot.
 * - Directly controlling to that value leads to overshoot and jitter.
 *
 * We therefore model:
 *   T_ntc   : raw sensor temperature from STATUS (hot spot)
 *   bias    : delta between hot spot and chamber "core" temperature
 *   T_core  : estimated chamber core temperature (used for UI + control)
 *
 * Update strategy:
 * - New STATUS -> store latest T_ntc
 * - oven_tick() (1 Hz) -> update bias PT1 and core PT1 deterministically
 *
 * Equations (discrete PT1 at 1 Hz):
 *   x = x + alpha * (x_target - x)   with alpha = dt / (tau + dt), dt=1s
 *
 * bias_target:
 *   heater_intent_on  -> heatBiasC (+ optional slope vs. (T_ntc - target))
 *   heater_intent_off -> coolBiasC (typically ~0)
 *
 * core_target:
 *   core_target = T_ntc - bias
 *
 * Heater pulse shaping:
 * - heaterWanted is derived from (T_core vs target ± tolerance) + safety gates.
 * - pulse gating enforces a fixed ON window and OFF rest to avoid chattering.
 *
 * Safety:
 * - Door open => heater OFF (best effort).
 * - Overtemp lock (latched) => heater OFF until below (target - tol).
 * - Comm loss => SAFE STOP (SET 0x0000) best effort.
 *
 * =============================================================================
 */


#include "oven_utils.h" // includes "oven.h"
// =============================================================================
// Includes
// =============================================================================

#include <Arduino.h>

// =============================================================================
// T11: Thermal Model (internal, oven.cpp only)
// =============================================================================
namespace {
struct ThermalModelState {
    float ntcC = 25.0f;        // latest measured sensor near heater (STATUS source)
    float coreC = 25.0f;       // estimated core temperature (filtered + bias)
    bool coreValid = false;    // becomes true after first init/telemetry
    uint32_t lastUpdateMs = 0; // for UI/diag

    // Pulse scheduler (ms-based) for heater request gating (T11.4)
    uint32_t heatPhaseUntilMs = 0; // if now < heatPhaseUntilMs => allow heater ON
    uint32_t restUntilMs = 0;      // if now < restUntilMs => force heater OFF
};

static ThermalModelState g_therm;

static void thermal_model_init(float initialNtcC, ThermalModelState &tms) {
    tms.ntcC = initialNtcC;
    tms.coreC = initialNtcC;
    tms.coreValid = true;
    tms.lastUpdateMs = millis();
    tms.heatPhaseUntilMs = 0;
    tms.restUntilMs = 0;
}

static void thermal_model_on_new_ntc(float ntcC, ThermalModelState &tms) {
    tms.ntcC = ntcC;
    if (!tms.coreValid) {
        thermal_model_init(ntcC, tms);
    }
}

static void thermal_model_update_pt1_1hz(const ThermalModelConfig &cfg,
                                         bool heaterIntentOn,
                                         ThermalModelState &tms) {
    if (!tms.coreValid) {
        return;
    }

    const float dt = 1.0f; // oven_tick is 1 Hz
    const float tau = (cfg.coreTauSeconds > 0.1f) ? cfg.coreTauSeconds : 0.1f;

    // Numerically stable PT1: alpha = dt / (tau + dt)
    const float alpha = dt / (tau + dt);

    const float bias = heaterIntentOn ? cfg.heatBiasC : cfg.coolBiasC;
    const float target = tms.ntcC + bias;

    tms.coreC = tms.coreC + alpha * (target - tms.coreC);
    tms.lastUpdateMs = millis();
}

static void thermal_model_copy_to_runtime(const ThermalModelState &tms, OvenRuntimeState &rt) {
    rt.tempNtcC = tms.ntcC;
    rt.tempCoreC = tms.coreC;
    rt.tempCoreValid = tms.coreValid;
    rt.lastThermalUpdateMs = tms.lastUpdateMs;
}

static void thermal_pulse_reset(ThermalModelState &tms) {
    tms.heatPhaseUntilMs = 0;
    tms.restUntilMs = 0;
}

static uint32_t clamp_u32_min(uint32_t v, uint32_t vmin) {
    return (v < vmin) ? vmin : v;
}

static uint32_t add_ms_sat(uint32_t a, uint32_t b) {
    const uint32_t r = a + b;
    return (r < a) ? 0xFFFFFFFFu : r; // saturate on overflow
}

// Returns whether we are allowed to set HEATER ON right now (pulse gating),
// given that "heaterWanted" is true.
static bool thermal_pulse_allow_heater_now(const ThermalModelConfig &cfg,
                                           uint32_t nowMs,
                                           bool heaterWanted,
                                           ThermalModelState &tms) {
    if (!heaterWanted) {
        thermal_pulse_reset(tms);
        return false;
    }

    // Heating window active (must win over restUntilMs, which is set to heatEnd+restEnd)
    if (tms.heatPhaseUntilMs != 0 && nowMs < tms.heatPhaseUntilMs) {
        return true;
    }

    // Rest phase (starts only after heat window ended)
    if (tms.restUntilMs != 0 && nowMs < tms.restUntilMs) {
        return false;
    }

    // Start a new heating window now
    const uint32_t heatWinMs = clamp_u32_min(cfg.heatWindowMs, 100u);
    const uint32_t restMs = clamp_u32_min(cfg.restMs, 100u);

    tms.heatPhaseUntilMs = add_ms_sat(nowMs, heatWinMs);
    tms.restUntilMs = add_ms_sat(tms.heatPhaseUntilMs, restMs);
    return true;
}

static void thermal_apply_ui_temp(OvenRuntimeState &rt) {
    // UI uses tempCurrent everywhere.
    // From T11.2 on: tempCurrent becomes "display/control temperature"
    // Raw NTC remains in tempNtcC.
    if (rt.tempCoreValid) {
        rt.tempCurrent = rt.tempCoreC;
    } else {
        rt.tempCurrent = rt.tempNtcC;
    }
}
} // namespace

// =============================================================================
// Internal static state (host-side)
// =============================================================================

static OvenRuntimeState preWaitSnapshot = {};
static bool hasPreWaitSnapshot = false;

// Legacy WAIT flag (transitional)
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

// Tunables
static constexpr uint32_t kAliveTimeoutMs = 1500;
static constexpr uint32_t kPingIntervalUnsyncedMs = 250;
static constexpr uint32_t kPingIntervalSyncedMs = 1000;
static constexpr uint16_t OVEN_TICK_MS = 1000;

static bool g_heaterIntentOn = false; // Host heater intent (for UI + thermal model)

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
static constexpr uint32_t kCommAliveTimeoutMs = 1500;

static bool g_hostOvertempActive = false;


// -------------------------------------------------------------------------
// T13 Safety / Relay timing
// -------------------------------------------------------------------------
static constexpr float HOTSPOT_MAX_C   = 140.0f; // safety sensor cutoff (near heater)
static constexpr float CHAMBER_MAX_C   = 120.0f; // absolute fail-safe limit
static constexpr float OVERSHOOT_CAP_C = 2.0f;   // cap above target (filament protection)

static constexpr uint32_t MIN_HEATER_ON_MS  = 2000; // relay-safe minimum ON time
static constexpr uint32_t MIN_HEATER_OFF_MS = 3000; // relay-safe minimum OFF time

static OvenProfile currentProfile = {
    .durationMinutes = 60,
    .targetTemperature = 45.0f,
    .filamentId = 0};

static PostConfig g_currentPostPlan = {false, 0, PostFanMode::FAST};

// =============================================================================
// UI-facing runtime state (Single Source of Truth for rendering)
// =============================================================================

static OvenRuntimeState runtimeState = {
    .durationMinutes = 60,
    .secondsRemaining = 60 * 60,

    .tempCurrent = 25.0f,
    .tempTarget = 40.0f,
    .tempToleranceC = 3.0f,
    .hostOvertempActive = false,

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

    .lastStatusAgeMs = 0xFFFFFFFFu,
    .lastRxAnyAgeMs = 0xFFFFFFFFu,
    .statusRxCount = 0,
    .commErrorCount = 0,

    .commAlive = false,
    .linkSynced = false,

    .mode = OvenMode::STOPPED,
    .running = false,

    .post = {false, 0, 0},

    // T11
    .tempNtcC = 25.0f,
    .tempCoreC = 25.0f,
    .tempCoreValid = false,
    .lastThermalUpdateMs = 0,
};

// =============================================================================
// HostComm integration (UART protocol, T6)
// =============================================================================

// =============================================================================
// HostComm / UART communication state
// =============================================================================
static HostComm *g_hostComm = nullptr;
static bool g_hasRealTelemetry = false;
static uint32_t g_lastStatusRequestMs = 0;

// =============================================================================
// Bitmask helpers
// =============================================================================
static inline uint16_t connector_u16(OVEN_CONNECTOR c) { return static_cast<uint16_t>(c); }

static inline bool mask_has(uint16_t mask, OVEN_CONNECTOR c) { return (mask & connector_u16(c)) != 0u; }

static inline uint16_t mask_set(uint16_t mask, OVEN_CONNECTOR c, bool on) {
    if (on) {
        return static_cast<uint16_t>(mask | connector_u16(c));
    }
    return static_cast<uint16_t>(mask & ~connector_u16(c));
}

static inline uint16_t preserve_inputs(uint16_t mask) {
    const bool door = mask_has(g_remoteOutputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);
    mask = mask_set(mask, OVEN_CONNECTOR::DOOR_ACTIVE, door);
    return mask;
}

// static inline void comm_send_mask(uint16_t newMask) {
//     if (!g_hostComm) {
//         return;
//     }
//     newMask = preserve_inputs(newMask);
//     g_lastCommandMask = newMask;

//     g_hostComm->setOutputsMask(newMask);
// }

static inline void comm_send_mask(uint16_t newMask) {
    if (!g_hostComm) {
        return;
    }

    newMask = preserve_inputs(newMask);

    g_lastCommandMask = newMask;
    g_heaterIntentOn = mask_has(newMask, OVEN_CONNECTOR::HEATER);
    g_hostComm->setOutputsMask(newMask);

    // Optimistic UI update (host intent). Will be corrected by ACK/STATUS telemetry.
    runtimeState.fan12v_on = mask_has(newMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(newMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(newMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(newMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_on = mask_has(newMask, OVEN_CONNECTOR::HEATER);
    runtimeState.lamp_on = mask_has(newMask, OVEN_CONNECTOR::LAMP);
}

// =============================================================================
// Telemetry -> runtime mapping
// =============================================================================
static void apply_remote_status_to_runtime(const ProtocolStatus &st) {
    // T13: Dual-NTC
    // - Chamber temperature is the ONLY control/UI temperature.
    // - Hotspot temperature is used ONLY for safety supervision.
    runtimeState.tempCurrent = static_cast<float>(st.tempChamber_dC) / 10.f;
    runtimeState.tempNtcC    = static_cast<float>(st.tempHotspot_dC) / 10.f;

    // Legacy T11 fields are kept in the struct for now but are no longer used.
    runtimeState.tempCoreC = runtimeState.tempCurrent;
    runtimeState.tempCoreValid = false;

    runtimeState.fan12v_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_on = mask_has(st.outputsMask, OVEN_CONNECTOR::HEATER);
    runtimeState.lamp_on = mask_has(st.outputsMask, OVEN_CONNECTOR::LAMP);
    runtimeState.door_open = mask_has(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);

    // For UI responsiveness during short heater pulses, reflect host intent while RUNNING.
    if (runtimeState.mode == OvenMode::RUNNING) {
        runtimeState.heater_on = g_heaterIntentOn;
    }
    g_hasRealTelemetry = true;
    g_remoteOutputsMask = st.outputsMask;

    g_lastStatusRxMs = millis();
    g_statusRxCount++;
}

static void force_local_safe_stop_due_to_comm(const char *reason) {
    (void)reason;

    runtimeState.mode = OvenMode::STOPPED;
    runtimeState.running = false;
    waiting = false;

    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    runtimeState.fan12v_on = false;
    runtimeState.fan230_on = false;
    runtimeState.fan230_slow_on = false;
    runtimeState.motor_on = false;
    runtimeState.heater_on = false;
    runtimeState.lamp_on = false;

    runtimeState.commAlive = false;
    runtimeState.linkSynced = false;
}

// =============================================================================
// Basic API
// =============================================================================
int oven_get_current_preset_index(void) { return runtimeState.filamentId; }

const FilamentPreset *oven_get_preset(uint16_t index) {
    if (index >= kPresetCount) {
        return &kPresets[0];
    }
    return &kPresets[index];
}

void oven_init(void) {
    // T11: init thermal model from current runtime snapshot
    thermal_model_init(runtimeState.tempCurrent, g_therm);
    thermal_model_copy_to_runtime(g_therm, runtimeState);
    thermal_apply_ui_temp(runtimeState);
    OVEN_INFO("[OVEN] Init OK\n");
}

void oven_start(void) {
    if (runtimeState.mode == OvenMode::RUNNING) {
        return;
    }
    if (runtimeState.mode == OvenMode::WAITING) {
        return;
    }

    runtimeState.mode = OvenMode::RUNNING;
    runtimeState.running = true;

    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;

    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    // Reset pulse scheduler on fresh start
    thermal_pulse_reset(g_therm);

    uint16_t m = g_remoteOutputsMask;
    m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    comm_send_mask(m);

    OVEN_INFO("[oven_start()]\n");
}

void oven_stop(void) {
    if (runtimeState.mode == OvenMode::STOPPED) {
        return;
    }

    runtimeState.mode = OvenMode::STOPPED;
    runtimeState.running = false;
    waiting = false;

    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    // Reset pulse scheduler on stop
    thermal_pulse_reset(g_therm);

    uint16_t m = g_remoteOutputsMask;
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);
    m = mask_set(m, OVEN_CONNECTOR::LAMP, false);
    comm_send_mask(m);

    OVEN_INFO("[oven_stop]\n");
}

bool oven_is_running(void) { return runtimeState.mode == OvenMode::RUNNING; }

uint16_t oven_get_preset_count(void) { return kPresetCount; }

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

void oven_select_preset(uint16_t index) {
    if (index >= kPresetCount) {
        return;
    }
    const FilamentPreset &p = kPresets[index];

    currentProfile.durationMinutes = p.durationMin;
    currentProfile.targetTemperature = p.dryTempC;
    currentProfile.filamentId = index;

    runtimeState.durationMinutes = p.durationMin;
    runtimeState.secondsRemaining = p.durationMin * 60;
    runtimeState.tempTarget = p.dryTempC;
    runtimeState.filamentId = index;
    runtimeState.rotaryOn = p.rotaryOn;

    g_currentPostPlan = p.post;

    strncpy(runtimeState.presetName, p.name, sizeof(runtimeState.presetName) - 1);
    runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

    OVEN_INFO("[oven_select_preset] Preset selected: %s\n", runtimeState.presetName);
}

void oven_get_runtime_state(OvenRuntimeState *out) {
    if (!out) {
        return;
    }
    *out = runtimeState;
}

// =============================================================================
// Timebase / countdown / diagnostics
// =============================================================================

// =============================================================================
// oven_tick(): 1 Hz timebase
// - countdown
// - deterministic T11 thermal model update
// - RUN -> POST -> STOP transitions
// =============================================================================
void oven_tick(void) {
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < OVEN_TICK_MS) {
        return;
    }
    lastTick = now;

    // T13: no thermal model update in oven_tick()

    // Countdown
    if (runtimeState.mode == OvenMode::RUNNING) {
        if (runtimeState.durationMinutes > 0) {
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            } else {
                if (g_currentPostPlan.active && g_currentPostPlan.seconds > 0) {
                    runtimeState.mode = OvenMode::POST;
                    thermal_pulse_reset(g_therm);
                    runtimeState.running = false;

                    runtimeState.post.active = true;
                    runtimeState.post.secondsRemaining = g_currentPostPlan.seconds;
                    runtimeState.post.stepIndex = 0;

                    runtimeState.secondsRemaining = g_currentPostPlan.seconds;
                    runtimeState.durationMinutes = (g_currentPostPlan.seconds + 59u) / 60u;

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

                    OVEN_INFO("[oven_tick] RUN->POST (seconds=%u)\n",
                              (unsigned)runtimeState.post.secondsRemaining);
                } else {
                    OVEN_INFO("[oven_tick] RUN finished -> STOP (no POST)\n");

                    oven_stop();
                }
            }
        }
    } else if (runtimeState.mode == OvenMode::POST) {
        if (runtimeState.post.active && runtimeState.post.secondsRemaining > 0) {
            runtimeState.post.secondsRemaining--;
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            }
        } else {
            oven_stop();
            OVEN_INFO("[oven_tick] POST finished -> STOP\n");
        }
    }

    runtimeState.statusRxCount = g_statusRxCount;
    runtimeState.commErrorCount = g_commErrorCount;

    // Mirror host safety latch
    runtimeState.hostOvertempActive = g_hostOvertempActive;
}

// =============================================================================
// Manual Overrides (UI -> requests -> SET mask)
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

// =============================================================================
// WAIT / RESUME (host-side)
// =============================================================================
bool oven_is_waiting(void) { return runtimeState.mode == OvenMode::WAITING; }

bool oven_is_alive(void) {
    if (g_hostComm != nullptr && g_hostComm->linkSynced()) {
        return true;
    }
    return false;
}

void oven_pause_wait(void) {
    if (runtimeState.mode != OvenMode::RUNNING || runtimeState.mode == OvenMode::WAITING) {
        OVEN_WARN("[oven_pause_wait] runtimeState.running=%d || waiting=%d\n",
                  runtimeState.running, waiting);
        return;
    }

    preWaitSnapshot = runtimeState;
    hasPreWaitSnapshot = true;

    g_preWaitCommandMask = g_lastCommandMask;

    runtimeState.mode = OvenMode::WAITING;
    runtimeState.running = false;

    // Reset pulse scheduler while waiting (heater must be off anyway)
    thermal_pulse_reset(g_therm);

    uint16_t m = g_remoteOutputsMask;
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
    m = mask_set(m, OVEN_CONNECTOR::LAMP, true);
    comm_send_mask(m);

    OVEN_INFO("[oven_pause_wait] WAIT (paused)\n");
}

bool oven_resume_from_wait(void) {
    if (runtimeState.mode != OvenMode::WAITING) {
        OVEN_WARN("[oven_resume_from_wait] waiting=%d\n", waiting);
        return false;
    }

    if (runtimeState.door_open) {
        OVEN_WARN("[oven_resume_from_wait] RESUME blocked: door open\n");
        return false;
    }

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
    runtimeState.running = true;
    waiting = false;

    // Reset pulse scheduler on resume to avoid immediate long ON stretches
    thermal_pulse_reset(g_therm);

    comm_send_mask(g_preWaitCommandMask);

    OVEN_INFO("[oven_resume_from_wait] RESUME from WAIT\n");
    return true;
}

// =============================================================================
// Runtime adjustments (non-persistent)
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
// Actuator setters (legacy)
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
// Communication (HostComm)
// =============================================================================
void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx) {
    static HostComm comm(serial);
    g_hostComm = &comm;

    g_hostComm->begin(baudrate, rx, tx);

    g_hasRealTelemetry = false;
    g_lastStatusRequestMs = 0;

    OVEN_INFO("[oven_comm_init] HostComm init OK\n");

    g_prevLinkSynced = false;
    g_lastRxMs = millis();
    g_safeStopSent = false;
    g_lastPingMs = 0;
}

// =============================================================================
// oven_comm_poll(): fast non-blocking comm loop (called frequently)
// - UART RX processing
// - Link sync & alive tracking
// - STATUS polling
// - Applies telemetry to runtime state
// - Applies heater policy (T11) while RUNNING
// =============================================================================

void oven_comm_poll(void) {
    if (!g_hostComm) {
        return;
    }

    uint32_t now = millis();

    // 1) Always read UART non-blocking
    g_hostComm->loop();
    now = millis();

    // 2) Mirror comm diagnostics into runtime
    runtimeState.linkSynced = g_hostComm->linkSynced();

    const uint32_t lastRxAny = g_hostComm->lastRxAnyMs();
    const uint32_t lastStatus = g_hostComm->lastStatusMs();

    runtimeState.lastRxAnyAgeMs = (lastRxAny > 0) ? (now - lastRxAny) : 0xFFFFFFFFu;
    runtimeState.lastStatusAgeMs = (lastStatus > 0) ? (now - lastStatus) : 0xFFFFFFFFu;

    const bool alive = (lastRxAny > 0) && ((now - lastRxAny) <= kAliveTimeoutMs);
    runtimeState.commAlive = alive;

    // 3) Handshake + keep-alive PING
    const uint32_t pingInterval = runtimeState.linkSynced ? kPingIntervalSyncedMs : kPingIntervalUnsyncedMs;
    if (now - g_lastPingMs >= pingInterval) {
        g_lastPingMs = now;
        g_hostComm->sendPing();
    }

    // 4) LinkSynced rising edge -> SAFE STOP once per sync-session
    if (!runtimeState.linkSynced) {
        g_sentSafeStopOnThisSync = false;
    }

    if (runtimeState.linkSynced && !g_sentSafeStopOnThisSync) {
        OVEN_WARN("[oven_comm_poll] LinkSynced -> SAFE STOP once (SET 0x0000)\n");
        comm_send_mask(0x0000);
        g_sentSafeStopOnThisSync = true;
    }

    // 5) Poll STATUS periodically (only when link is synced AND alive)
    if (runtimeState.linkSynced && runtimeState.commAlive) {
        if (now - g_lastStatusRequestMs >= kStatusPollIntervalMs) {
            g_lastStatusRequestMs = now;
            g_hostComm->requestStatus();
        }
    }

    // 6) Apply new telemetry
    if (g_hostComm->hasNewStatus()) {
        apply_remote_status_to_runtime(g_hostComm->getRemoteStatus());
        g_hostComm->clearNewStatusFlag();
    }

    // -------------------------------------------------------------------------
// T13: Heater control + safety (RUNNING only)
// - Control temperature: runtimeState.tempCurrent (Chamber)
// - Safety temperature:  runtimeState.tempNtcC (Hotspot)
// - Simple hysteresis: ON below (tgt - tol), OFF above (tgt + tol)
// - Safety cutoffs force heater OFF
// - Relay-safe timing guard prevents rapid toggling
// -------------------------------------------------------------------------
if (runtimeState.mode == OvenMode::RUNNING) {
    const float chamberC = runtimeState.tempCurrent;
    const float hotspotC = runtimeState.tempNtcC;
    const float tgt = runtimeState.tempTarget;
    const float tol = runtimeState.tempToleranceC;

    // Safety evaluation (non-latching; becomes true while any condition is violated)
    bool safety = false;

    if (hotspotC >= HOTSPOT_MAX_C) {
        safety = true;
    }
    if (chamberC >= CHAMBER_MAX_C) {
        safety = true;
    }
    if (chamberC >= (tgt + OVERSHOOT_CAP_C)) {
        safety = true;
    }

    runtimeState.safetyCutoffActive = safety;

    // Door open: best-effort force heater OFF
    if (runtimeState.door_open) {
        safety = true;
    }

    // Hysteresis decision (desired heater intent)
    const float lo = tgt - tol;
    const float hi = tgt + tol;

    bool desiredHeater = false;
    if (!safety) {
        if (g_heaterIntentOn) {
            // currently ON -> turn OFF when above hi
            desiredHeater = (chamberC <= hi);
        } else {
            // currently OFF -> turn ON when below lo
            desiredHeater = (chamberC < lo);
        }
    }

    // Relay timing guard
    static uint32_t s_lastHeaterSwitchMs = 0;
    const uint32_t nowMs = millis();
    const uint32_t since = (s_lastHeaterSwitchMs > 0) ? (nowMs - s_lastHeaterSwitchMs) : 0xFFFFFFFFu;

    if (desiredHeater != g_heaterIntentOn) {
        const uint32_t minHold = g_heaterIntentOn ? MIN_HEATER_ON_MS : MIN_HEATER_OFF_MS;
        if (since >= minHold) {
            s_lastHeaterSwitchMs = nowMs;
            g_heaterIntentOn = desiredHeater;
        }
    }

    // Apply heater intent to command mask (best-effort; remote truth is still telemetry)
    uint16_t cmd = g_lastCommandMask;
    cmd = mask_set(cmd, OVEN_CONNECTOR::HEATER, g_heaterIntentOn);

    // Overtemp indicator mirrors safety for now (kept for existing UI/logic)
    g_hostOvertempActive = runtimeState.safetyCutoffActive;
    runtimeState.hostOvertempActive = g_hostOvertempActive;

    comm_send_mask(cmd);
}
 else {
        // Not RUNNING: clear latch, stop pulses
        g_hostOvertempActive = false;
        thermal_pulse_reset(g_therm);
    }

    // 7) ACK-based outputs update (fast UI feedback)
    if (g_hostComm->lastSetAcked() || g_hostComm->lastUpdAcked() || g_hostComm->lastTogAcked()) {
        const ProtocolStatus &st = g_hostComm->getRemoteStatus();
        uint16_t mask = preserve_inputs(st.outputsMask);

        runtimeState.fan12v_on = mask_has(mask, OVEN_CONNECTOR::FAN12V);
        runtimeState.fan230_on = mask_has(mask, OVEN_CONNECTOR::FAN230V);
        runtimeState.fan230_slow_on = mask_has(mask, OVEN_CONNECTOR::FAN230V_SLOW);
        runtimeState.motor_on = mask_has(mask, OVEN_CONNECTOR::SILICAT_MOTOR);
        runtimeState.heater_on = mask_has(mask, OVEN_CONNECTOR::HEATER);
        runtimeState.lamp_on = mask_has(mask, OVEN_CONNECTOR::LAMP);
        runtimeState.door_open = mask_has(mask, OVEN_CONNECTOR::DOOR_ACTIVE);

        // For UI responsiveness during short heater pulses, reflect host intent while RUNNING.
        if (runtimeState.mode == OvenMode::RUNNING) {
            runtimeState.heater_on = g_heaterIntentOn;
        }

        g_remoteOutputsMask = mask;

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
    if ((lastRxAny > 0) && !alive) {
        if (!g_safeStopSent) {
            OVEN_WARN("[oven_comm_poll] Alive timeout (%lums) -> SAFE STOP (SET 0x0000)\n",
                      (unsigned long)(now - lastRxAny));

            comm_send_mask(0x0000);
            g_safeStopSent = true;

            force_local_safe_stop_due_to_comm("alive-timeout");
            g_remoteOutputsMask = 0;
        }

        g_hostComm->clearLinkSync();
        g_sentSafeStopOnThisSync = false;
    } else {
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

// =============================================================================
// Debug HW toggles + force outputs off
// =============================================================================
void oven_dbg_hw_toggle_by_index(int idx) {
    if (!g_hostComm) {
        return;
    }
    if (!g_hostComm->linkSynced()) {
        OVEN_WARN("[oven_dbg_hw_toggle_by_index] ignored: not synced\n");
        return;
    }

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

    if (c == OVEN_CONNECTOR::FAN230V && next) {
        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    }
    if (c == OVEN_CONNECTOR::FAN230V_SLOW && next) {
        m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    }

    comm_send_mask(m);
    g_hostComm->requestStatus();

    OVEN_INFO("[oven_dbg_hw_toggle_by_index] idx=%d -> %s (mask=%s)\n",
              idx, next ? "ON" : "OFF", oven_outputs_mask_to_str(m));
}

void oven_force_outputs_off(void) {
    comm_send_mask(0x0000);
    g_remoteOutputsMask = 0;
    OVEN_WARN("[oven] force outputs OFF\n");
}