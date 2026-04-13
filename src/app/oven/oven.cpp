/**
 * =============================================================================
 * oven.cpp — Host-side oven logic
 * =============================================================================
 *
 * Product architecture:
 * - Host (ESP32-S3): UI, runtime state, heater decision, countdown and policy
 * - Client (ESP32-WROOM): authoritative IO, sensor acquisition and safety gating
 * - UART protocol (HostComm): line-based ASCII frames with CRLF
 *
 * Single Source of Truth:
 * - UI renders from OvenRuntimeState
 * - Client telemetry is the source of truth for effective actuator states
 * - Chamber temperature is the control temperature
 * - Hotspot temperature is the safety temperature
 *
 * Safety:
 * - Door open => heater OFF (best effort)
 * - Chamber overtemp => heater OFF
 * - Hotspot overtemp => heater OFF
 * - Comm loss => SAFE STOP (SET 0x0000) best effort
 *
 * Relay protection:
 * - Host decides heater request ON/OFF
 * - Minimum ON/OFF times are enforced before switching the effective heater state
 *
 * =============================================================================
 */

#include "host_parameters.h"
#include "oven_utils.h" // includes "oven.h"
// =============================================================================
// Includes
// =============================================================================

#include "log_csv.h"
#include <Arduino.h>

static constexpr int16_t TEMP_INVALID_DC = -32768;

namespace {
struct HeaterGateState {
    uint32_t heatPhaseUntilMs = 0; // if now < heatPhaseUntilMs => allow heater ON
    uint32_t restUntilMs = 0;      // if now < restUntilMs => force heater OFF
    uint8_t pulseCount = 0;
    uint32_t nextPulseOverrideMs = 0;
};

struct FanGateState {
    bool fastFanActive = false;
    uint32_t forceFastUntilMs = 0;
    uint32_t lastSwitchMs = 0;
};

static HeaterGateState g_heaterGate;
static FanGateState g_fanGate;

static void thermal_pulse_reset(HeaterGateState &tms) {
    tms.heatPhaseUntilMs = 0;
    tms.restUntilMs = 0;
    tms.pulseCount = 0;
    tms.nextPulseOverrideMs = 0;
}

static void fan_gate_reset(FanGateState &fgs) {
    fgs.fastFanActive = false;
    fgs.forceFastUntilMs = 0;
    fgs.lastSwitchMs = 0;
}

static bool heater_gate_is_heating(const HeaterGateState &tms, uint32_t nowMs) {
    return (tms.heatPhaseUntilMs > 0) && (nowMs < tms.heatPhaseUntilMs);
}

static bool heater_gate_is_resting(const HeaterGateState &tms, uint32_t nowMs) {
    return (tms.restUntilMs > 0) && (nowMs < tms.restUntilMs);
}

static void heater_gate_begin_heat(HeaterGateState &tms, uint32_t nowMs, uint32_t pulseMs) {
    tms.heatPhaseUntilMs = nowMs + pulseMs;
    tms.restUntilMs = 0;
    tms.pulseCount++;
}

static void heater_gate_begin_rest(HeaterGateState &tms, uint32_t nowMs, uint32_t soakMs) {
    tms.heatPhaseUntilMs = 0;
    tms.restUntilMs = nowMs + soakMs;
}

static void fan_gate_force_fast(FanGateState &fgs, uint32_t nowMs, uint32_t holdMs) {
    if (holdMs == 0) {
        return;
    }

    if (!fgs.fastFanActive) {
        fgs.fastFanActive = true;
        fgs.lastSwitchMs = nowMs;
    }

    const uint32_t holdUntilMs = nowMs + holdMs;
    if (holdUntilMs > fgs.forceFastUntilMs) {
        fgs.forceFastUntilMs = holdUntilMs;
    }
}
} // namespace

// =============================================================================
// Internal static state (host-side)
// =============================================================================
static constexpr uint32_t HEATER_MIN_ON_MS = 2000;
static constexpr uint32_t HEATER_MIN_OFF_MS = 2000;

static bool g_heaterEffectiveOn = false;
static uint32_t g_heaterLastSwitchMs = 0;
static uint32_t g_waitStartedMs = 0;

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

static HeaterPolicy g_lowTempHeaterPolicy = {
    HeaterMaterialClass::FILAMENT,
    HOST_HEATER_HYSTERESIS_C,
    10.0f,
    4.0f,
    HOST_TARGET_OVERSHOOT_CAP_C,
    HOST_CHAMBER_MAX_C,
    HOST_HOTSPOT_MAX_C,
};

static HeaterPolicy g_midTempHeaterPolicy = {
    HeaterMaterialClass::FILAMENT,
    HOST_HEATER_HYSTERESIS_C,
    10.0f,
    4.0f,
    HOST_TARGET_OVERSHOOT_CAP_C,
    HOST_CHAMBER_MAX_C,
    HOST_HOTSPOT_MAX_C,
};

static HeaterPolicy g_highTempHeaterPolicy = {
    HeaterMaterialClass::FILAMENT,
    HOST_HEATER_HYSTERESIS_C,
    10.0f,
    4.0f,
    HOST_TARGET_OVERSHOOT_CAP_C,
    HOST_CHAMBER_MAX_C,
    HOST_HOTSPOT_MAX_C,
};

static HeaterPolicy g_silica100HeaterPolicy = {
    HeaterMaterialClass::SILICA,
    HOST_SILICA_HEATER_HYSTERESIS_C,
    10.0f,
    2.5f,
    HOST_SILICA_TARGET_OVERSHOOT_CAP_C,
    HOST_CHAMBER_MAX_C,
    HOST_HOTSPOT_MAX_C,
};

// -------------------------------------------------------------------------
// T16 HELPER
// -------------------------------------------------------------------------
static bool compute_heater_effective(bool requestOn) {
    const uint32_t now = millis();
    const uint32_t dt = now - g_heaterLastSwitchMs;

    if (g_heaterEffectiveOn) {
        if (!requestOn && dt >= HEATER_MIN_ON_MS) {
            g_heaterEffectiveOn = false;
            g_heaterLastSwitchMs = now;
        }
    } else {
        if (requestOn && dt >= HEATER_MIN_OFF_MS) {
            g_heaterEffectiveOn = true;
            g_heaterLastSwitchMs = now;
        }
    }

    return g_heaterEffectiveOn;
}

// T16.Host.1.1 uses HEATER_MIN_ON_MS / HEATER_MIN_OFF_MS above.
static constexpr uint16_t kDefaultPresetIndex = 3; // ASA

static OvenProfile currentProfile = {
    .durationMinutes = 300,
    .targetTemperature = 82.5f,
    .filamentId = kDefaultPresetIndex};

static PostConfig g_currentPostPlan = {false, 0, PostFanMode::FAST};

// =============================================================================
// UI-facing runtime state (Single Source of Truth for rendering)
// =============================================================================

static OvenRuntimeState runtimeState = {
    .durationMinutes = 300,
    .secondsRemaining = 300 * 60,

    .tempCurrent = 25.0f,
    .tempChamberC = 25.0f,
    .tempTarget = 82.5f,
    .tempToleranceC = HOST_HEATER_HYSTERESIS_C,
    .hostOvertempActive = false,
    .safetyCutoffActive = false,
    .tempHotspotC = 25.0f,
    .tempChamberValid = false,
    .tempHotspotValid = false,
    .materialClass = HeaterMaterialClass::FILAMENT,
    .heaterCurveProfile = HeaterCurveProfileId::HIGH_80C,
    .heaterStage = HeaterControlStage::IDLE,

    .filamentId = kDefaultPresetIndex,
    .cooldownMinutes = 5,

    .fan12v_on = false,
    .fan230_on = false,
    .fan230_slow_on = false,
    .heater_on = false,
    .heater_request_on = false,
    .heater_actual_on = false,
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
    .delayStart = {false, 0, 0},
    .delayStartRuntime = {false, false, false, 0},

    // Temporary legacy alias
    .tempNtcC = 25.0f,
};

static uint8_t sanitize_delay_start_minutes(uint8_t minutes) {
    if (minutes > 59u) {
        return 59u;
    }
    return minutes;
}

static void sync_post_plan_from_runtime(void) {
    g_currentPostPlan.active = (runtimeState.cooldownMinutes > 0);
    g_currentPostPlan.seconds = static_cast<uint16_t>(runtimeState.cooldownMinutes * 60u);
}

static void clear_delay_start_runtime(void) {
    runtimeState.delayStartRuntime.active = false;
    runtimeState.delayStartRuntime.waiting = false;
    runtimeState.delayStartRuntime.paused = false;
    runtimeState.delayStartRuntime.delayRemainingSec = 0;
}

static void runtime_sync_legacy_temperature_aliases() {
    runtimeState.tempCurrent = runtimeState.tempChamberC;
    runtimeState.tempNtcC = runtimeState.tempHotspotC;
}

static void runtime_sync_heater_alias() {
    runtimeState.heater_on =
        (runtimeState.mode == OvenMode::RUNNING) ? runtimeState.heater_request_on
                                                 : runtimeState.heater_actual_on;
}

static void sync_heater_policies_from_host_parameters() {
    const HostParameters *params = host_parameters_get_cached();
    if (!params) {
        return;
    }

    g_lowTempHeaterPolicy.hysteresisC = params->heaterProfiles[0].hysteresis_dC / 10.0f;
    g_lowTempHeaterPolicy.approachBandC = params->heaterProfiles[0].approachBand_dC / 10.0f;
    g_lowTempHeaterPolicy.holdBandC = params->heaterProfiles[0].holdBand_dC / 10.0f;
    g_lowTempHeaterPolicy.targetOvershootCapC = params->heaterProfiles[0].overshootCap_dC / 10.0f;

    g_midTempHeaterPolicy.hysteresisC = params->heaterProfiles[1].hysteresis_dC / 10.0f;
    g_midTempHeaterPolicy.approachBandC = params->heaterProfiles[1].approachBand_dC / 10.0f;
    g_midTempHeaterPolicy.holdBandC = params->heaterProfiles[1].holdBand_dC / 10.0f;
    g_midTempHeaterPolicy.targetOvershootCapC = params->heaterProfiles[1].overshootCap_dC / 10.0f;

    g_highTempHeaterPolicy.hysteresisC = params->heaterProfiles[2].hysteresis_dC / 10.0f;
    g_highTempHeaterPolicy.approachBandC = params->heaterProfiles[2].approachBand_dC / 10.0f;
    g_highTempHeaterPolicy.holdBandC = params->heaterProfiles[2].holdBand_dC / 10.0f;
    g_highTempHeaterPolicy.targetOvershootCapC = params->heaterProfiles[2].overshootCap_dC / 10.0f;

    g_silica100HeaterPolicy.hysteresisC = params->heaterProfiles[3].hysteresis_dC / 10.0f;
    g_silica100HeaterPolicy.approachBandC = params->heaterProfiles[3].approachBand_dC / 10.0f;
    g_silica100HeaterPolicy.holdBandC = params->heaterProfiles[3].holdBand_dC / 10.0f;
    g_silica100HeaterPolicy.targetOvershootCapC = params->heaterProfiles[3].overshootCap_dC / 10.0f;
}

static HostSilicaPulseParameters silica_pulse_parameters(void) {
    HostSilicaPulseParameters pulse = {
        HOST_SILICA_REHEAT_SOAK_MS,
        HOST_SILICA_HOLD_PULSE_MAX_MS,
        static_cast<int16_t>(HOST_SILICA_REHEAT_ENABLE_BELOW_TARGET_C * 10.0f),
        static_cast<int16_t>(HOST_SILICA_FORCE_OFF_BEFORE_TARGET_C * 10.0f),
    };

    const HostParameters *params = host_parameters_get_cached();
    if (params) {
        pulse = params->silicaPulse;
    }
    return pulse;
}

float oven_get_effective_preset_target_c(uint16_t index) {
    if (index >= kPresetCount) {
        return 0.0f;
    }

    const FilamentPreset &preset = kPresets[index];
    if (std::strcmp(preset.name, "CUSTOM") == 0) {
        return preset.dryTempC;
    }

    const HostParameters *params = host_parameters_get_cached();
    if (!params) {
        return preset.dryTempC;
    }

    const uint8_t profile_index = static_cast<uint8_t>(preset.heaterCurveProfile);
    if (profile_index >= HOST_PARAMETER_HEATER_PROFILE_COUNT) {
        return preset.dryTempC;
    }

    return static_cast<float>(params->heaterProfiles[profile_index].targetC);
}

static HeaterMaterialClass material_class_from_preset_index(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(kPresetCount)) {
        return HeaterMaterialClass::FILAMENT;
    }
    return kPresets[presetIndex].materialClass;
}

static HeaterCurveProfileId heater_curve_profile_from_preset_index(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(kPresetCount)) {
        return HeaterCurveProfileId::LOW_45C;
    }
    return kPresets[presetIndex].heaterCurveProfile;
}

static const HeaterPolicy &heater_policy_for_profile(HeaterCurveProfileId profileId) {
    sync_heater_policies_from_host_parameters();

    switch (profileId) {
    case HeaterCurveProfileId::LOW_45C:
        return g_lowTempHeaterPolicy;
    case HeaterCurveProfileId::MID_60C:
        return g_midTempHeaterPolicy;
    case HeaterCurveProfileId::HIGH_80C:
        return g_highTempHeaterPolicy;
    case HeaterCurveProfileId::SILICA_100C:
        return g_silica100HeaterPolicy;
    default:
        return g_lowTempHeaterPolicy;
    }
}

static const HeaterPolicy &active_heater_policy() {
    return heater_policy_for_profile(runtimeState.heaterCurveProfile);
}

static HeaterControlStage determine_heater_stage(float chamberC,
                                                 float targetC,
                                                 const HeaterPolicy &policy) {
    const float deltaToTargetC = targetC - chamberC;
    if (deltaToTargetC > policy.approachBandC) {
        return HeaterControlStage::BULK_HEAT;
    }
    if (deltaToTargetC > policy.holdBandC) {
        return HeaterControlStage::APPROACH;
    }
    return HeaterControlStage::HOLD;
}

static float heater_stage_band_c(HeaterControlStage stage, const HeaterPolicy &policy) {
    switch (stage) {
    case HeaterControlStage::BULK_HEAT:
        return policy.approachBandC;
    case HeaterControlStage::APPROACH:
        return policy.holdBandC;
    case HeaterControlStage::HOLD:
        return policy.hysteresisC;
    case HeaterControlStage::IDLE:
    default:
        return policy.hysteresisC;
    }
}

static bool determine_heater_intent_for_stage(HeaterControlStage stage,
                                              bool currentIntentOn,
                                              float chamberC,
                                              float targetC,
                                              const HeaterPolicy &policy) {
    switch (stage) {
    case HeaterControlStage::BULK_HEAT:
        return true;

    case HeaterControlStage::APPROACH:
        if (currentIntentOn) {
            return chamberC < (targetC - policy.holdBandC);
        }
        return chamberC < (targetC - policy.approachBandC);

    case HeaterControlStage::HOLD:
        if (currentIntentOn) {
            return chamberC < targetC;
        }
        return chamberC < (targetC - policy.hysteresisC);

    case HeaterControlStage::IDLE:
    default:
        return false;
    }
}

static bool silica_should_force_heater_off(float chamberC,
                                           float targetC,
                                           uint8_t pulseCount) {
    const HostSilicaPulseParameters pulse = silica_pulse_parameters();
    const bool firstPulseActive = (pulseCount <= 1u);
    const float forceOffBeforeTargetC =
        firstPulseActive ? HOST_SILICA_FIRST_PULSE_FORCE_OFF_BEFORE_TARGET_C
                         : (pulse.forceOffBeforeTarget_dC / 10.0f);
    return chamberC >= (targetC - forceOffBeforeTargetC);
}

static bool filament_should_force_heater_off(HeaterControlStage stage,
                                             float chamberC,
                                             float hotspotC,
                                             float targetC,
                                             uint8_t pulseCount) {
    if (hotspotC >= (targetC + HOST_FILAMENT_HOTSPOT_FORCE_OFF_ABOVE_TARGET_C)) {
        return true;
    }

    const bool firstPulseActive = (pulseCount <= 1u);
    const float directForceOffBeforeTargetC =
        firstPulseActive ? HOST_FILAMENT_FIRST_PULSE_FORCE_OFF_BEFORE_TARGET_C
                         : HOST_FILAMENT_FORCE_OFF_BEFORE_TARGET_C;
    if (chamberC >= (targetC - directForceOffBeforeTargetC)) {
        return true;
    }

    const float hotspotLeadC = max(0.0f, hotspotC - chamberC);
    const float predictedChamberC = chamberC + (hotspotLeadC * 1.5f);
    const float predictiveForceOffBeforeTargetC =
        firstPulseActive ? HOST_FILAMENT_FIRST_PULSE_FORCE_OFF_BEFORE_TARGET_C : 0.5f;

    if (predictedChamberC >= (targetC - predictiveForceOffBeforeTargetC)) {
        return true;
    }

    if (stage == HeaterControlStage::HOLD && hotspotC >= targetC) {
        return true;
    }

    return false;
}

static bool is_high_temp_filament_profile(HeaterCurveProfileId profileId) {
    return profileId == HeaterCurveProfileId::HIGH_80C;
}

static float filament_reheat_enable_below_target_c(HeaterCurveProfileId profileId) {
    return is_high_temp_filament_profile(profileId)
               ? HOST_FILAMENT_HIGH_REHEAT_ENABLE_BELOW_TARGET_C
               : HOST_FILAMENT_REHEAT_ENABLE_BELOW_TARGET_C;
}

static uint32_t filament_hold_pulse_duration_ms(float targetC,
                                                HeaterCurveProfileId profileId) {
    if (targetC >= HOST_FILAMENT_WAIT_RESUME_HOT_TARGET_C) {
        return is_high_temp_filament_profile(profileId)
                   ? HOST_FILAMENT_HOLD_PULSE_HIGH_MS
                   : HOST_FILAMENT_HOLD_PULSE_MAX_MS;
    }
    if (targetC >= HOST_FILAMENT_MID_TARGET_C) {
        return HOST_FILAMENT_HOLD_PULSE_MID_MS;
    }
    return HOST_FILAMENT_HOLD_PULSE_WARM_MS;
}

static uint32_t filament_pulse_duration_ms(float chamberC,
                                           float targetC,
                                           uint8_t pulseCount,
                                           HeaterCurveProfileId profileId) {
    if (g_heaterGate.nextPulseOverrideMs > 0) {
        const uint32_t pulseMs = g_heaterGate.nextPulseOverrideMs;
        g_heaterGate.nextPulseOverrideMs = 0;
        return pulseMs;
    }

    if (pulseCount == 0) {
        if (targetC >= HOST_FILAMENT_WAIT_RESUME_HOT_TARGET_C) {
            return HOST_FILAMENT_FIRST_PULSE_MAX_MS;
        }
        if (targetC >= HOST_FILAMENT_MID_TARGET_C) {
            return HOST_FILAMENT_FIRST_PULSE_MAX_MID_MS;
        }
        return HOST_FILAMENT_FIRST_PULSE_MAX_WARM_MS;
    }

    const float errorToTargetC = targetC - chamberC;
    if (errorToTargetC > HOST_FILAMENT_BULK_PULSE_ENABLE_BELOW_TARGET_C) {
        return HOST_FILAMENT_BULK_PULSE_MAX_MS;
    }
    if (errorToTargetC > HOST_FILAMENT_APPROACH_PULSE_ENABLE_BELOW_TARGET_C) {
        return HOST_FILAMENT_APPROACH_PULSE_MAX_MS;
    }
    if (errorToTargetC > filament_reheat_enable_below_target_c(profileId)) {
        return filament_hold_pulse_duration_ms(targetC, profileId);
    }
    return 0;
}

static uint32_t filament_soak_duration_ms(uint8_t pulseCount) {
    return (pulseCount <= 1u) ? HOST_FILAMENT_FIRST_SOAK_MS
                              : HOST_FILAMENT_REHEAT_SOAK_MS;
}

static uint32_t silica_pulse_duration_ms(float chamberC, float targetC, uint8_t pulseCount) {
    const HostSilicaPulseParameters pulse = silica_pulse_parameters();
    const float errorToTargetC = targetC - chamberC;

    if (pulseCount == 0) {
        return HOST_SILICA_FIRST_PULSE_MAX_MS;
    }
    if (errorToTargetC > HOST_SILICA_BULK_PULSE_ENABLE_BELOW_TARGET_C) {
        return HOST_SILICA_BULK_PULSE_MAX_MS;
    }
    if (errorToTargetC > HOST_SILICA_APPROACH_PULSE_ENABLE_BELOW_TARGET_C) {
        return HOST_SILICA_APPROACH_PULSE_MAX_MS;
    }
    if (errorToTargetC > (pulse.reheatEnableBelowTarget_dC / 10.0f)) {
        return pulse.holdPulseMaxMs;
    }
    return 0;
}

static uint32_t silica_soak_duration_ms(uint8_t pulseCount) {
    const HostSilicaPulseParameters pulse = silica_pulse_parameters();
    return (pulseCount <= 1u) ? HOST_SILICA_FIRST_SOAK_MS
                              : pulse.reheatSoakMs;
}

static bool silica_reheat_allowed(float chamberC, float targetC) {
    const HostSilicaPulseParameters pulse = silica_pulse_parameters();
    return chamberC < (targetC - (pulse.reheatEnableBelowTarget_dC / 10.0f));
}

static bool determine_silica_heater_intent(float chamberC, float targetC) {
    const uint32_t nowMs = millis();

    if (heater_gate_is_heating(g_heaterGate, nowMs)) {
        return true;
    }

    if ((g_heaterGate.heatPhaseUntilMs > 0) && (nowMs >= g_heaterGate.heatPhaseUntilMs)) {
        heater_gate_begin_rest(g_heaterGate, nowMs,
                               silica_soak_duration_ms(g_heaterGate.pulseCount));
    }

    if (heater_gate_is_resting(g_heaterGate, nowMs)) {
        return false;
    }

    if (!silica_reheat_allowed(chamberC, targetC)) {
        return false;
    }

    const uint32_t pulseMs =
        silica_pulse_duration_ms(chamberC, targetC, g_heaterGate.pulseCount);
    if (pulseMs == 0) {
        return false;
    }

    heater_gate_begin_heat(g_heaterGate, nowMs, pulseMs);
    return true;
}

static bool filament_reheat_allowed(float chamberC,
                                    float hotspotC,
                                    float targetC,
                                    HeaterCurveProfileId profileId) {
    if (chamberC >= (targetC - filament_reheat_enable_below_target_c(profileId))) {
        return false;
    }
    if (hotspotC >= (targetC + HOST_FILAMENT_HOTSPOT_REHEAT_BLOCK_ABOVE_TARGET_C)) {
        return false;
    }
    return true;
}

static bool determine_filament_heater_intent(float chamberC,
                                             float hotspotC,
                                             float targetC,
                                             HeaterCurveProfileId profileId) {
    const uint32_t nowMs = millis();

    if (heater_gate_is_heating(g_heaterGate, nowMs)) {
        return true;
    }

    if ((g_heaterGate.heatPhaseUntilMs > 0) && (nowMs >= g_heaterGate.heatPhaseUntilMs)) {
        heater_gate_begin_rest(g_heaterGate, nowMs,
                               filament_soak_duration_ms(g_heaterGate.pulseCount));
    }

    if (heater_gate_is_resting(g_heaterGate, nowMs)) {
        return false;
    }

    if (!filament_reheat_allowed(chamberC, hotspotC, targetC, profileId)) {
        return false;
    }

    const uint32_t pulseMs =
        filament_pulse_duration_ms(chamberC, targetC, g_heaterGate.pulseCount, profileId);
    if (pulseMs == 0) {
        return false;
    }

    heater_gate_begin_heat(g_heaterGate, nowMs, pulseMs);
    return true;
}

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

static void apply_filament_running_fan_policy(uint16_t &cmd, bool heaterEffective) {
    const uint32_t nowMs = millis();
    const bool shouldUseFastFan =
        !heaterEffective && (nowMs < g_fanGate.forceFastUntilMs);

    if (shouldUseFastFan != g_fanGate.fastFanActive &&
        ((nowMs - g_fanGate.lastSwitchMs) >= HOST_FILAMENT_FAN_MIN_SWITCH_MS)) {
        g_fanGate.fastFanActive = shouldUseFastFan;
        g_fanGate.lastSwitchMs = nowMs;
    }

    cmd = mask_set(cmd, OVEN_CONNECTOR::FAN12V, true);
    cmd = mask_set(cmd, OVEN_CONNECTOR::FAN230V, g_fanGate.fastFanActive);
    cmd = mask_set(cmd, OVEN_CONNECTOR::FAN230V_SLOW, !g_fanGate.fastFanActive);
}

static uint32_t filament_resume_soak_ms(float chamberC, float targetC, uint32_t waitOpenMs) {
    uint32_t soakMs = HOST_FILAMENT_WAIT_RESUME_SOAK_MS;

    if (targetC >= HOST_FILAMENT_WAIT_RESUME_HOT_TARGET_C) {
        soakMs = HOST_FILAMENT_WAIT_RESUME_SOAK_HOT_TARGET_MS;
    }

    const float errorToTargetC = max(0.0f, targetC - chamberC);
    if (errorToTargetC >= HOST_FILAMENT_WAIT_RESUME_LONG_PULSE_ERROR_C) {
        soakMs = min(soakMs, static_cast<uint32_t>(7000));
    } else if (errorToTargetC >= HOST_FILAMENT_WAIT_RESUME_MEDIUM_PULSE_ERROR_C) {
        soakMs = min(soakMs, static_cast<uint32_t>(9000));
    }

    if (waitOpenMs >= HOST_FILAMENT_WAIT_RESUME_LONG_OPEN_MS) {
        soakMs = max(HOST_FILAMENT_WAIT_RESUME_SOAK_MIN_MS, soakMs - 2000u);
    }

    return max(HOST_FILAMENT_WAIT_RESUME_SOAK_MIN_MS, soakMs);
}

static uint32_t filament_resume_pulse_ms(float chamberC, float targetC) {
    const float errorToTargetC = max(0.0f, targetC - chamberC);
    uint32_t pulseMs = HOST_FILAMENT_WAIT_RESUME_PULSE_SHORT_MS;

    if (errorToTargetC >= HOST_FILAMENT_WAIT_RESUME_LONG_PULSE_ERROR_C) {
        pulseMs = HOST_FILAMENT_WAIT_RESUME_PULSE_LONG_MS;
    } else if (errorToTargetC >= HOST_FILAMENT_WAIT_RESUME_MEDIUM_PULSE_ERROR_C) {
        pulseMs = (HOST_FILAMENT_WAIT_RESUME_PULSE_SHORT_MS +
                   HOST_FILAMENT_WAIT_RESUME_PULSE_LONG_MS) /
                  2u;
    }

    if (targetC >= HOST_FILAMENT_WAIT_RESUME_HOT_TARGET_C) {
        const uint32_t minPulseMs = HOST_FILAMENT_WAIT_RESUME_PULSE_SHORT_MS - 1000u;
        pulseMs = (pulseMs > 1000u) ? (pulseMs - 1000u) : minPulseMs;
        if (pulseMs < minPulseMs) {
            pulseMs = minPulseMs;
        }
    }

    return pulseMs;
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

    // Optimistic host-side command view. Remote truth still comes from STATUS/ACK.
    runtimeState.fan12v_on = mask_has(newMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(newMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(newMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(newMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_request_on = g_heaterIntentOn;
    runtimeState.lamp_on = mask_has(newMask, OVEN_CONNECTOR::LAMP);
    runtime_sync_heater_alias();
}

static inline void comm_send_mask_if_changed(uint16_t newMask) {
    const uint16_t normalizedMask = preserve_inputs(newMask);
    if (normalizedMask == g_lastCommandMask) {
        return;
    }
    comm_send_mask(normalizedMask);
}

static bool host_heater_safety_cutoff_active(const OvenRuntimeState &state) {
    const HeaterPolicy &policy = heater_policy_for_profile(state.heaterCurveProfile);
    if (state.door_open) {
        return true;
    }
    if (state.tempHotspotC >= policy.hotspotMaxC) {
        return true;
    }
    if (state.tempChamberC >= policy.chamberMaxC) {
        return true;
    }
    if (state.tempChamberC >= (state.tempTarget + policy.targetOvershootCapC)) {
        return true;
    }
    return false;
}

static uint8_t oven_mode_to_u8(OvenMode mode) {
    return static_cast<uint8_t>(mode);
}

static uint8_t heater_material_class_to_u8(HeaterMaterialClass materialClass) {
    return static_cast<uint8_t>(materialClass);
}

static uint8_t heater_stage_to_u8(HeaterControlStage stage) {
    return static_cast<uint8_t>(stage);
}

static int32_t c_to_dC(float tempC) {
    return static_cast<int32_t>(tempC * 10.0f);
}

static void emit_csv_host_runtime_once_per_second(const OvenRuntimeState &state) {
    static uint32_t lastMs = 0;
    const uint32_t now = millis();

    if ((now - lastMs) < 1000u) {
        return;
    }
    lastMs = now;

    const float lowC = state.tempTarget - state.tempToleranceC;
    const float highC = state.tempTarget + state.tempToleranceC;

    CSV_LOG_HOST_TEMP(
        (long)c_to_dC(state.tempChamberC),
        (long)c_to_dC(state.tempHotspotC),
        (long)c_to_dC(state.tempTarget),
        (long)c_to_dC(lowC),
        (long)c_to_dC(highC),
        state.safetyCutoffActive ? 1 : 0);

    CSV_LOG_HOST_LOGIC(
        (unsigned)oven_mode_to_u8(state.mode),
        state.running ? 1 : 0,
        state.heater_request_on ? 1 : 0,
        state.heater_actual_on ? 1 : 0,
        state.door_open ? 1 : 0,
        state.safetyCutoffActive ? 1 : 0,
        state.commAlive ? 1 : 0,
        state.linkSynced ? 1 : 0,
        (unsigned)heater_material_class_to_u8(state.materialClass),
        (unsigned)heater_stage_to_u8(state.heaterStage));
}

// =============================================================================
// Telemetry -> runtime mapping
// =============================================================================
static void apply_remote_status_to_runtime(const ProtocolStatus &st) {
    // T16/T13 explicit naming:
    // - Chamber temperature is the ONLY control/UI temperature.
    // - Hotspot temperature is used ONLY for safety supervision.
    runtimeState.tempChamberValid = (st.tempChamber_dC != TEMP_INVALID_DC);
    runtimeState.tempHotspotValid = (st.tempHotspot_dC != TEMP_INVALID_DC);

    runtimeState.tempChamberC = runtimeState.tempChamberValid
                                    ? (static_cast<float>(st.tempChamber_dC) / 10.f)
                                    : runtimeState.tempChamberC;
    runtimeState.tempHotspotC = runtimeState.tempHotspotValid
                                    ? (static_cast<float>(st.tempHotspot_dC) / 10.f)
                                    : runtimeState.tempHotspotC;
    runtime_sync_legacy_temperature_aliases();

    runtimeState.fan12v_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_has(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.motor_on = mask_has(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    runtimeState.heater_actual_on = mask_has(st.outputsMask, OVEN_CONNECTOR::HEATER);
    runtimeState.heater_request_on = g_heaterIntentOn;
    runtimeState.lamp_on = mask_has(st.outputsMask, OVEN_CONNECTOR::LAMP);
    runtimeState.door_open = mask_has(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);
    runtime_sync_heater_alias();
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
    g_heaterIntentOn = false;
    g_heaterEffectiveOn = false;
    runtimeState.heater_request_on = false;
    runtimeState.heater_actual_on = false;
    runtime_sync_heater_alias();
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
    oven_select_preset(kDefaultPresetIndex);
    runtimeState.materialClass = material_class_from_preset_index(currentProfile.filamentId);
    runtimeState.heaterCurveProfile = heater_curve_profile_from_preset_index(currentProfile.filamentId);
    runtimeState.tempToleranceC = active_heater_policy().hysteresisC;
    sync_post_plan_from_runtime();
    runtime_sync_legacy_temperature_aliases();
    runtime_sync_heater_alias();
    OVEN_INFO("[OVEN] Init OK\n");
}

void oven_start(void) {
    if (runtimeState.mode == OvenMode::RUNNING) {
        return;
    }
    if (runtimeState.mode == OvenMode::WAITING) {
        return;
    }

    clear_delay_start_runtime();

    runtimeState.mode = OvenMode::RUNNING;
    runtimeState.running = true;

    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;
    runtimeState.materialClass = material_class_from_preset_index(currentProfile.filamentId);
    runtimeState.heaterCurveProfile = heater_curve_profile_from_preset_index(currentProfile.filamentId);
    runtimeState.tempToleranceC = active_heater_policy().hysteresisC;
    runtimeState.heaterStage = HeaterControlStage::BULK_HEAT;

    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    // Start in a cold, deterministic heater state. The first ON decision must
    // come from the normal control path after current telemetry is evaluated.
    thermal_pulse_reset(g_heaterGate);
    fan_gate_reset(g_fanGate);
    g_heaterIntentOn = false;
    g_heaterEffectiveOn = false;
    runtimeState.heater_request_on = false;
    runtimeState.heater_actual_on = false;
    runtime_sync_heater_alias();

    uint16_t m = g_remoteOutputsMask;
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, runtimeState.rotaryOn);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    comm_send_mask(m);

    OVEN_INFO("[oven_start()]\n");
}

void oven_stop(void) {
    if (runtimeState.mode == OvenMode::STOPPED &&
        !runtimeState.delayStartRuntime.active) {
        return;
    }

    runtimeState.mode = OvenMode::STOPPED;
    runtimeState.running = false;
    runtimeState.heaterStage = HeaterControlStage::IDLE;
    waiting = false;
    clear_delay_start_runtime();

    runtimeState.post.active = false;
    runtimeState.post.secondsRemaining = 0;
    runtimeState.post.stepIndex = 0;

    // Reset pulse scheduler on stop
    thermal_pulse_reset(g_heaterGate);
    fan_gate_reset(g_fanGate);
    g_heaterIntentOn = false;
    g_heaterEffectiveOn = false;
    runtimeState.heater_request_on = false;
    runtimeState.heater_actual_on = false;
    runtime_sync_heater_alias();

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

bool oven_start_or_schedule(void) {
    if (runtimeState.mode == OvenMode::RUNNING ||
        runtimeState.mode == OvenMode::WAITING ||
        runtimeState.mode == OvenMode::POST ||
        runtimeState.delayStartRuntime.active) {
        return false;
    }

    if (!runtimeState.delayStart.enabled) {
        oven_start();
        return true;
    }

    const uint32_t delaySeconds = oven_get_delay_start_seconds();
    if (delaySeconds == 0u) {
        oven_start();
        return true;
    }

    clear_delay_start_runtime();
    runtimeState.delayStartRuntime.active = true;
    runtimeState.delayStartRuntime.waiting = true;
    runtimeState.delayStartRuntime.delayRemainingSec = delaySeconds;

    OVEN_INFO("[oven_start_or_schedule] delayed start armed: %lu sec\n",
              (unsigned long)delaySeconds);
    return true;
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
    const float effective_target_c = oven_get_effective_preset_target_c(index);

    currentProfile.durationMinutes = p.durationMin;
    currentProfile.targetTemperature = effective_target_c;
    currentProfile.filamentId = index;

    runtimeState.durationMinutes = p.durationMin;
    runtimeState.secondsRemaining = p.durationMin * 60;
    runtimeState.tempTarget = effective_target_c;
    runtimeState.filamentId = index;
    runtimeState.materialClass = p.materialClass;
    runtimeState.heaterCurveProfile = p.heaterCurveProfile;
    runtimeState.tempToleranceC = heater_policy_for_profile(p.heaterCurveProfile).hysteresisC;
    runtimeState.rotaryOn = p.rotaryOn;
    runtimeState.cooldownMinutes = p.cooldownMinutes;

    g_currentPostPlan = p.post;
    sync_post_plan_from_runtime();

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
// - countdown
// - RUN -> POST -> STOP transitions
// =============================================================================
void oven_tick(void) {
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < OVEN_TICK_MS) {
        return;
    }
    lastTick = now;

    // Countdown
    if (runtimeState.mode == OvenMode::RUNNING) {
        if (runtimeState.durationMinutes > 0) {
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            } else {
                if (g_currentPostPlan.active && g_currentPostPlan.seconds > 0) {
                    runtimeState.mode = OvenMode::POST;
                    thermal_pulse_reset(g_heaterGate);
                    fan_gate_reset(g_fanGate);
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

                    const bool useFastCooldownFan =
                        (runtimeState.materialClass == HeaterMaterialClass::FILAMENT);

                    if (!useFastCooldownFan && g_currentPostPlan.fanMode == PostFanMode::SLOW) {
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

    if (runtimeState.delayStartRuntime.active &&
        runtimeState.delayStartRuntime.waiting &&
        !runtimeState.delayStartRuntime.paused) {
        if (runtimeState.delayStartRuntime.delayRemainingSec > 0) {
            runtimeState.delayStartRuntime.delayRemainingSec--;
        }

        if (runtimeState.delayStartRuntime.delayRemainingSec == 0) {
            runtimeState.delayStartRuntime.active = false;
            runtimeState.delayStartRuntime.waiting = false;
            runtimeState.delayStartRuntime.paused = false;
            OVEN_INFO("[oven_tick] delayed start expired -> RUNNING\n");
            oven_start();
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

void oven_set_delay_start(uint8_t hours, uint8_t minutes, bool enabled) {
    if (hours > 23u) {
        hours = 23u;
    }

    runtimeState.delayStart.enabled = enabled;
    runtimeState.delayStart.delayStartHours = hours;
    runtimeState.delayStart.delayStartMinutes = sanitize_delay_start_minutes(minutes);

    OVEN_INFO("[oven_set_delay_start] enabled=%d delay=%02u:%02u\n",
              enabled ? 1 : 0,
              (unsigned)runtimeState.delayStart.delayStartHours,
              (unsigned)runtimeState.delayStart.delayStartMinutes);
}

void oven_get_delay_start_config(DelayStartConfig *out) {
    if (!out) {
        return;
    }

    *out = runtimeState.delayStart;
}

uint32_t oven_get_delay_start_seconds(void) {
    return (static_cast<uint32_t>(runtimeState.delayStart.delayStartHours) * 3600u) +
           (static_cast<uint32_t>(runtimeState.delayStart.delayStartMinutes) * 60u);
}

void oven_set_preset_cooldown_minutes(uint16_t minutes) {
    if (minutes > 30u) {
        minutes = 30u;
    }

    minutes = static_cast<uint16_t>((minutes / 5u) * 5u);
    runtimeState.cooldownMinutes = minutes;
    sync_post_plan_from_runtime();

    OVEN_INFO("[oven_set_preset_cooldown_minutes] cooldown=%u min\n",
              (unsigned)runtimeState.cooldownMinutes);
}

bool oven_delay_start_is_active(void) {
    return runtimeState.delayStartRuntime.active;
}

bool oven_delay_start_is_waiting(void) {
    return runtimeState.delayStartRuntime.active && runtimeState.delayStartRuntime.waiting;
}

bool oven_delay_start_pause(void) {
    if (!oven_delay_start_is_waiting() || runtimeState.delayStartRuntime.paused) {
        return false;
    }

    runtimeState.delayStartRuntime.paused = true;
    OVEN_INFO("[oven_delay_start_pause] paused at %lu sec\n",
              (unsigned long)runtimeState.delayStartRuntime.delayRemainingSec);
    return true;
}

bool oven_delay_start_resume(void) {
    if (!oven_delay_start_is_waiting() || !runtimeState.delayStartRuntime.paused) {
        return false;
    }

    runtimeState.delayStartRuntime.paused = false;
    OVEN_INFO("[oven_delay_start_resume] resumed at %lu sec\n",
              (unsigned long)runtimeState.delayStartRuntime.delayRemainingSec);
    return true;
}

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
    g_waitStartedMs = millis();

    // Reset pulse scheduler while waiting (heater must be off anyway)
    thermal_pulse_reset(g_heaterGate);
    fan_gate_reset(g_fanGate);

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
        runtimeState.materialClass = preWaitSnapshot.materialClass;
        runtimeState.tempToleranceC = preWaitSnapshot.tempToleranceC;
        runtimeState.heaterStage = preWaitSnapshot.heaterStage;
        runtimeState.rotaryOn = preWaitSnapshot.rotaryOn;

        std::strncpy(runtimeState.presetName, preWaitSnapshot.presetName,
                     sizeof(runtimeState.presetName) - 1);
        runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

        runtimeState.secondsRemaining = keepSeconds;
    }

    runtimeState.mode = OvenMode::RUNNING;
    runtimeState.running = true;
    runtimeState.heaterStage = HeaterControlStage::BULK_HEAT;
    waiting = false;

    const uint32_t now = millis();
    const uint32_t waitOpenMs = (g_waitStartedMs > 0) ? (now - g_waitStartedMs) : 0;

    // Filament resume after a door-open WAIT must not behave like a cold start.
    // Recovery is based on current chamber error, door-open time and target band
    // so hotter presets can resume slightly faster than low-temp filament runs.
    if (runtimeState.materialClass == HeaterMaterialClass::FILAMENT) {
        thermal_pulse_reset(g_heaterGate);
        g_heaterGate.pulseCount = 1;
        g_heaterGate.restUntilMs =
            now + filament_resume_soak_ms(runtimeState.tempChamberC,
                                          runtimeState.tempTarget,
                                          waitOpenMs);
        g_heaterGate.nextPulseOverrideMs =
            filament_resume_pulse_ms(runtimeState.tempChamberC,
                                     runtimeState.tempTarget);
        runtimeState.heaterStage = HeaterControlStage::APPROACH;
    } else {
        // Silica keeps the simpler resume behavior for now.
        thermal_pulse_reset(g_heaterGate);
    }
    fan_gate_reset(g_fanGate);

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
// - Applies heater policy while RUNNING
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
    // T16/T13: Heater control + safety (RUNNING only)
    // - Control temperature: runtimeState.tempChamberC
    // - Safety temperature:  runtimeState.tempHotspotC
    // - Simple hysteresis: ON below (tgt - tol), OFF above (tgt + tol)
    // - Safety cutoffs force heater OFF
    // - Relay-safe timing guard prevents rapid toggling
    // -------------------------------------------------------------------------
    if (runtimeState.mode == OvenMode::RUNNING) {
        const float chamberC = runtimeState.tempChamberC;
        const float tgt = runtimeState.tempTarget;
        const HeaterPolicy &policy = active_heater_policy();
        const bool isFilament = (runtimeState.materialClass == HeaterMaterialClass::FILAMENT);
        const bool isSilica100 =
            (runtimeState.heaterCurveProfile == HeaterCurveProfileId::SILICA_100C);

        const bool safety = host_heater_safety_cutoff_active(runtimeState);
        runtimeState.safetyCutoffActive = safety;
        const HeaterControlStage stage = determine_heater_stage(chamberC, tgt, policy);
        runtimeState.heaterStage = stage;
        runtimeState.tempToleranceC = heater_stage_band_c(stage, policy);

        bool desiredHeater = false;
        if (!safety) {
            if (isFilament) {
                desiredHeater =
                    determine_filament_heater_intent(
                        chamberC, runtimeState.tempHotspotC, tgt, runtimeState.heaterCurveProfile);
            } else if (isSilica100) {
                desiredHeater = determine_silica_heater_intent(chamberC, tgt);
            } else {
                desiredHeater = determine_heater_intent_for_stage(
                    stage, g_heaterIntentOn, chamberC, tgt, policy);
            }

            if (desiredHeater && isFilament &&
                filament_should_force_heater_off(
                    stage, chamberC, runtimeState.tempHotspotC, tgt, g_heaterGate.pulseCount)) {
                desiredHeater = false;
                heater_gate_begin_rest(g_heaterGate, now, HOST_FILAMENT_REHEAT_SOAK_MS);
            } else if (desiredHeater && isSilica100 &&
                       silica_should_force_heater_off(chamberC, tgt, g_heaterGate.pulseCount)) {
                desiredHeater = false;
                heater_gate_begin_rest(g_heaterGate, now, silica_pulse_parameters().reheatSoakMs);
            }
        } else if (isFilament) {
            heater_gate_begin_rest(g_heaterGate, now, HOST_FILAMENT_SAFETY_SOAK_MS);
        } else if (isSilica100) {
            heater_gate_begin_rest(g_heaterGate, now, HOST_SILICA_SAFETY_SOAK_MS);
        }

        // Request = control decision, Effective = relay-safe output after minimum ON/OFF timing
        g_heaterIntentOn = desiredHeater;
        runtimeState.heater_request_on = g_heaterIntentOn;

        const bool wasHeaterEffective = g_heaterEffectiveOn;
        const bool heaterEffective = compute_heater_effective(g_heaterIntentOn);
        if (isFilament && wasHeaterEffective && !heaterEffective) {
            fan_gate_force_fast(g_fanGate, now, HOST_FILAMENT_FAN_FAST_AFTER_HEAT_MS);
        }
        runtimeState.heater_actual_on = heaterEffective;
        runtime_sync_heater_alias();

        // Apply relay-safe effective state to command mask (remote truth is still telemetry)
        uint16_t cmd = g_lastCommandMask;
        cmd = mask_set(cmd, OVEN_CONNECTOR::HEATER, heaterEffective);
        if (isFilament) {
            if (heaterEffective) {
                g_fanGate.forceFastUntilMs = 0;
            }
            apply_filament_running_fan_policy(cmd, heaterEffective);
        }

        // Overtemp indicator mirrors safety for now (kept for existing UI/logic)
        g_hostOvertempActive = runtimeState.safetyCutoffActive;
        runtimeState.hostOvertempActive = g_hostOvertempActive;

        comm_send_mask_if_changed(cmd);
    } else {
        // Not RUNNING: clear latch, stop pulses
        g_hostOvertempActive = false;
        g_heaterIntentOn = false;
        runtimeState.heaterStage = HeaterControlStage::IDLE;
        runtimeState.heater_request_on = false;
        g_heaterEffectiveOn = false;
        runtimeState.heater_actual_on = false;
        runtime_sync_heater_alias();
        thermal_pulse_reset(g_heaterGate);
        fan_gate_reset(g_fanGate);
    }

    // 7) ACK-based outputs update (fast UI feedback)
    if (g_hostComm->lastSetAcked() || g_hostComm->lastUpdAcked() || g_hostComm->lastTogAcked()) {
        const ProtocolStatus &st = g_hostComm->getRemoteStatus();
        uint16_t mask = preserve_inputs(st.outputsMask);

        runtimeState.fan12v_on = mask_has(mask, OVEN_CONNECTOR::FAN12V);
        runtimeState.fan230_on = mask_has(mask, OVEN_CONNECTOR::FAN230V);
        runtimeState.fan230_slow_on = mask_has(mask, OVEN_CONNECTOR::FAN230V_SLOW);
        runtimeState.motor_on = mask_has(mask, OVEN_CONNECTOR::SILICAT_MOTOR);
        runtimeState.heater_actual_on = mask_has(mask, OVEN_CONNECTOR::HEATER);
        runtimeState.heater_request_on = g_heaterIntentOn;
        runtimeState.lamp_on = mask_has(mask, OVEN_CONNECTOR::LAMP);
        runtimeState.door_open = mask_has(mask, OVEN_CONNECTOR::DOOR_ACTIVE);
        runtime_sync_heater_alias();

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

    emit_csv_host_runtime_once_per_second(runtimeState);
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
