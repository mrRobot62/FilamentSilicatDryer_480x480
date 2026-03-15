#pragma once
#include <stdint.h>

namespace hcc {

// Temperature unit: deci-degrees Celsius (dC). Example: 40.0°C -> 400 dC.
static constexpr int16_t TEMP_INVALID_DC = (int16_t)-32768;

enum class HeaterMode : uint8_t {
    SAFE_OFF = 0, // Door open or safety trip => always off
    HEAT_UP,      // Aggressive heat until we approach target
    APPROACH,     // Reduced energy close to target (avoid overshoot)
    HOLD          // Maintain in-band using windowed duty
};

struct HeaterControllerConfig {
    // Target band
    int16_t target_dC = 400;        // 40.0°C
    int16_t maxOvershoot_dC = 50;   // +5.0°C
    int16_t minUndershoot_dC = 100; // -10.0°C

    // Safety caps (controller-level, not a replacement for SafetyGuard/T14)
    int16_t abortTemp_dC = 1200; // 120.0°C hard cutoff

    // Hotspot-based soft cap to prevent early overshoot:
    // If hotspot rises too far above chamber, reduce/stop heating.
    int16_t hotspotDeltaMax_dC = 150; // +15.0°C above chamber
    int16_t hotspotAbsMax_dC = 1150;  // 115.0°C absolute cap (soft)

    // Prediction / smoothing
    // Chamber slope is low-pass filtered. This tau models "thermal lag" of chamber response.
    uint16_t slopeLpfAlpha_256 = 32; // 0..256 (higher = faster), 32 ~= 0.125
    uint16_t predictTau_s = 12;      // seconds of "expected remaining rise" after turning off

    // Hold windowing (simple, robust)
    uint16_t holdWindowMs = 2000; // 2s windows
    uint8_t minDutyHoldPct = 0;   // clamp lower bound
    uint8_t maxDutyHoldPct = 70;  // clamp upper bound in HOLD
};

struct HeaterControllerInput {
    uint32_t nowMs = 0;

    bool doorClosed = true;

    int16_t chamber_dC = TEMP_INVALID_DC;
    int16_t hotspot_dC = TEMP_INVALID_DC;
};

struct HeaterControllerOutput {
    HeaterMode mode = HeaterMode::SAFE_OFF;

    // Requested heater energy in percent (0..100).
    // The caller decides whether this maps to PWM duty or "window ON time".
    uint8_t dutyPercent = 0;

    // Diagnostics for UDP logs
    int16_t chamberSlope_dC_per_s = 0; // filtered slope
    int16_t predictedChamber_dC = TEMP_INVALID_DC;
    uint8_t reasons = 0; // bitfield (see ReasonBits)
};

struct HeaterControllerState {
    HeaterMode mode = HeaterMode::SAFE_OFF;

    // Slope estimation
    int16_t lastChamber_dC = TEMP_INVALID_DC;
    uint32_t lastChamberMs = 0;
    int32_t slopeFilt_dC_per_s = 0; // internal (scaled int)

    // HOLD window scheduler
    uint32_t windowStartMs = 0;
    uint16_t windowOnMs = 0;
};

// Reason bits (for output.reasons)
struct ReasonBits {
    static constexpr uint8_t DOOR_OPEN = 1u << 0;
    static constexpr uint8_t INVALID_SENSOR = 1u << 1;
    static constexpr uint8_t ABORT_TEMP = 1u << 2;
    static constexpr uint8_t HOTSPOT_CAP = 1u << 3;
    static constexpr uint8_t PREDICT_CUTOFF = 1u << 4;
    static constexpr uint8_t IN_BAND = 1u << 5;
};

class HeaterController {
  public:
    explicit HeaterController(const HeaterControllerConfig &cfg);

    void reset(uint32_t nowMs);
    HeaterControllerOutput update(const HeaterControllerInput &in);

    const HeaterControllerConfig &config() const { return cfg_; }
    const HeaterControllerState &state() const { return st_; }

  private:
    HeaterControllerConfig cfg_;
    HeaterControllerState st_;

    bool sensorsValid_(const HeaterControllerInput &in) const;

    void updateSlope_(const HeaterControllerInput &in);
    int16_t predictedChamber_(int16_t chamber_dC, int16_t slope_dC_per_s) const;

    // mode handlers
    HeaterControllerOutput runSafe_(const HeaterControllerInput &in, uint8_t reasons);
    HeaterControllerOutput runHeatUp_(const HeaterControllerInput &in);
    HeaterControllerOutput runApproach_(const HeaterControllerInput &in, int16_t slope_dC_per_s, int16_t pred_dC);
    HeaterControllerOutput runHold_(const HeaterControllerInput &in, int16_t slope_dC_per_s, int16_t pred_dC);

    // duty helper
    uint8_t clampDuty_(int32_t dutyPct, uint8_t minPct, uint8_t maxPct) const;

    // window scheduling for HOLD
    uint8_t dutyHoldToWindowPct_(int16_t err_dC) const;
    bool windowShouldBeOn_(uint32_t nowMs) const;
};

} // namespace hcc
