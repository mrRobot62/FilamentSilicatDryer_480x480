#include "oven_utils.h" // includes "oven.h"
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

    // Rest phase dominates
    if (tms.restUntilMs != 0 && nowMs < tms.restUntilMs) {
        return false;
    }

    // Heating window active
    if (tms.heatPhaseUntilMs != 0 && nowMs < tms.heatPhaseUntilMs) {
        return true;
    }

    // Start a new heating window now
    const uint32_t heatWinMs = clamp_u32_min(cfg.heatWindowMs, 100u);
    const uint32_t restMs = cfg.restMs;

    tms.heatPhaseUntilMs = add_ms_sat(nowMs, heatWinMs);
    tms.restUntilMs = add_ms_sat(tms.heatPhaseUntilMs, restMs);
    return true;
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

static OvenProfile currentProfile = {
    .durationMinutes = 60,
    .targetTemperature = 45.0f,
    .filamentId = 0};

static PostConfig g_currentPostPlan = {false, 0, PostFanMode::FAST};

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
static void apply_remote_status_to_runtime(const ProtocolStatus &st) {
    runtimeState.tempCurrent = static_cast<float>(st.tempRaw / 10.f);

    // T11 input: STATUS temperature is the NTC sensor near heater
    thermal_model_on_new_ntc(runtimeState.tempCurrent, g_therm);
    thermal_model_copy_to_runtime(g_therm, runtimeState);
    thermal_apply_ui_temp(runtimeState);

    runtimeState.fan12v_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_on = mask_has(st.outputsMask, OVEN_CONNECTOR::HEATER);
    runtimeState.lamp_on = mask_has(st.outputsMask, OVEN_CONNECTOR::LAMP);
    runtimeState.door_open = mask_has(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);

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
void oven_tick(void) {
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < OVEN_TICK_MS) {
        return;
    }
    lastTick = now;

    // T11: update PT1 core model at 1 Hz.
    // Use host "intent" (last command mask) for heater state, not telemetry (may lag).
    const bool heaterIntent = mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER);
    thermal_model_update_pt1_1hz(kThermalConfig, heaterIntent, g_therm);
    thermal_model_copy_to_runtime(g_therm, runtimeState);
    thermal_apply_ui_temp(runtimeState);

    // T11 - thermal - debug-log
    static uint32_t s_lastDbgMs = 0;
    const uint32_t dbgNow = millis();
    if (dbgNow - s_lastDbgMs >= 5000) {
        s_lastDbgMs = dbgNow;

        const bool heaterIntent = mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER);

        const int32_t heatRem = (g_therm.heatPhaseUntilMs != 0 && dbgNow < g_therm.heatPhaseUntilMs)
                                    ? (int32_t)(g_therm.heatPhaseUntilMs - dbgNow)
                                    : 0;
        const int32_t restRem = (g_therm.restUntilMs != 0 && dbgNow < g_therm.restUntilMs)
                                    ? (int32_t)(g_therm.restUntilMs - dbgNow)
                                    : 0;

        OVEN_INFO("[T11] ntc=%.2f core=%.2f ui=%.2f tgt=%.2f heaterIntent=%d heatRemMs=%ld restRemMs=%ld\n",
                  runtimeState.tempNtcC,
                  runtimeState.tempCoreC,
                  runtimeState.tempCurrent,
                  runtimeState.tempTarget,
                  heaterIntent ? 1 : 0,
                  (long)heatRem,
                  (long)restRem);
    }

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
    // T11: Heater request policy (RUNNING only)
    // - Uses tempCoreC (estimated) for decision, not raw tempCurrent.
    // - Pulse-gates heater via heatWindow/restMs (ms-based).
    // - Door open ALWAYS forces heater OFF (best-effort).
    // - Overtemp lock also forces OFF.
    // -------------------------------------------------------------------------
    if (runtimeState.mode == OvenMode::RUNNING) {
        // const float cur = runtimeState.tempCoreValid ? runtimeState.tempCoreC : runtimeState.tempCurrent;
        // const float tgt = runtimeState.tempTarget;
        // const float tol = runtimeState.tempToleranceC;

        // const float hi = tgt + tol;
        // const float lo = tgt - tol;

        // // Door safety: force OFF
        // if (runtimeState.door_open) {
        //     thermal_pulse_reset(g_therm);

        //     if (mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER)) {
        //         uint16_t m = g_lastCommandMask;
        //         m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
        //         comm_send_mask(m);
        //         OVEN_WARN("[SAFETY] DOOR OPEN -> HEATER OFF (host)\n");
        //     }
        // } else {
        //     // Overtemp lock (latched until <= lo)
        //     if (!g_hostOvertempActive && (cur >= hi)) {
        //         g_hostOvertempActive = true;
        //         OVEN_WARN("[SAFETY] OVER-TEMP -> lock active (cur=%.1f >= hi=%.1f)\n", cur, hi);
        //     } else if (g_hostOvertempActive && (cur <= lo)) {
        //         g_hostOvertempActive = false;
        //         OVEN_INFO("[SAFETY] OVER-TEMP recovered (cur=%.1f <= lo=%.1f)\n", cur, lo);
        //     }

        //     const bool heaterWanted = (!g_hostOvertempActive) && (cur <= lo);

        //     // Pulse gating decision
        //     bool heaterAllowedNow = thermal_pulse_allow_heater_now(kThermalConfig, now, heaterWanted, g_therm);

        //     // Hard OFF above hi (even if pulse says "on")
        //     if (cur >= hi) {
        //         heaterAllowedNow = false;
        //         thermal_pulse_reset(g_therm);
        //     }

        //     const bool heaterIsOn = mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER);

        //     if (heaterAllowedNow && !heaterIsOn) {
        //         uint16_t m = g_lastCommandMask;
        //         m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
        //         comm_send_mask(m);
        //         OVEN_INFO("[HEATER] ON  (core=%.1f, tgt=%.1f, tol=%.1f)\n", cur, tgt, tol);
        //     } else if (!heaterAllowedNow && heaterIsOn) {
        //         uint16_t m = g_lastCommandMask;
        //         m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
        //         comm_send_mask(m);
        //         OVEN_INFO("[HEATER] OFF (core=%.1f, tgt=%.1f, tol=%.1f, lock=%d)\n",
        //                   cur, tgt, tol, g_hostOvertempActive ? 1 : 0);
        //     }
        // }

        // -------------------------------------------------------------------------
        // T11.4: Heater request policy (RUNNING only)
        // - Uses tempCoreC (estimated) as control temperature if valid
        // - Hysteresis around target: ON below (tgt - tol), OFF above (tgt + tol)
        // - Pulse gating: heater can only be ON during heatWindowMs, then forced OFF for restMs
        // - Door open ALWAYS forces heater OFF (best-effort)
        // - Overtemp lockout forces OFF, clears only when <= (tgt - tol)
        // -------------------------------------------------------------------------

        const float cur = runtimeState.tempCoreValid ? runtimeState.tempCoreC : runtimeState.tempCurrent;
        const float tgt = runtimeState.tempTarget;
        const float tol = runtimeState.tempToleranceC;

        const float hi = tgt + tol;
        const float lo = tgt - tol;

        // 1) Door safety: force OFF and reset pulse scheduler
        if (runtimeState.door_open) {
            thermal_pulse_reset(g_therm);

            if (mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER)) {
                uint16_t m = g_lastCommandMask;
                m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
                comm_send_mask(m);
                OVEN_WARN("[SAFETY] DOOR OPEN -> HEATER OFF (host)\n");
            }
        } else {
            // 2) Overtemp lock (latched until <= lo)
            if (!g_hostOvertempActive && (cur >= hi)) {
                g_hostOvertempActive = true;
                OVEN_WARN("[SAFETY] OVER-TEMP -> lock active (core=%.2f >= hi=%.2f)\n", cur, hi);
            } else if (g_hostOvertempActive && (cur <= lo)) {
                g_hostOvertempActive = false;
                OVEN_INFO("[SAFETY] OVER-TEMP recovered (core=%.2f <= lo=%.2f)\n", cur, lo);
            }

            // 3) Base heater desire (hysteresis bottom)
            //    "Wanted" means: we are below lower threshold AND not locked.
            const bool heaterWanted = (!g_hostOvertempActive) && (cur <= lo);

            // 4) Pulse gating (ms-based)
            //    If heaterWanted -> allow ON only during heatWindowMs, then force OFF for restMs.
            //    If heaterWanted=false -> OFF and reset pulse state.
            bool heaterAllowedNow = thermal_pulse_allow_heater_now(kThermalConfig, now, heaterWanted, g_therm);

            // 5) Hard OFF at/above hi (top of hysteresis)
            //    This also resets pulse scheduler (prevents immediate re-ON).
            if (cur >= hi) {
                heaterAllowedNow = false;
                thermal_pulse_reset(g_therm);
            }

            // 6) Apply to command mask (intent), not telemetry
            const bool heaterIsOn = mask_has(g_lastCommandMask, OVEN_CONNECTOR::HEATER);

            if (heaterAllowedNow && !heaterIsOn) {
                uint16_t m = g_lastCommandMask;
                m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
                comm_send_mask(m);

                OVEN_INFO("[HEATER] ON  (core=%.2f tgt=%.2f tol=%.2f win=%lu rest=%lu)\n",
                          cur, tgt, tol,
                          (unsigned long)kThermalConfig.heatWindowMs,
                          (unsigned long)kThermalConfig.restMs);
            } else if (!heaterAllowedNow && heaterIsOn) {
                uint16_t m = g_lastCommandMask;
                m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
                comm_send_mask(m);

                OVEN_INFO("[HEATER] OFF (core=%.2f tgt=%.2f tol=%.2f lock=%d)\n",
                          cur, tgt, tol, g_hostOvertempActive ? 1 : 0);
            }
            // else: hold
        }

    } else {
        // Not RUNNING: clear latch, stop pulses
        g_hostOvertempActive = false;
        thermal_pulse_reset(g_therm);
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
