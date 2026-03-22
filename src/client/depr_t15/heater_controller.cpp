#include "depr_T15/heater_controller.h"

namespace hcc {

static inline int16_t clamp_i16(int32_t v, int16_t lo, int16_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return (int16_t)v;
}

HeaterController::HeaterController(const HeaterControllerConfig &cfg)
    : cfg_(cfg) {
    reset(0);
}

void HeaterController::reset(uint32_t nowMs) {
    st_ = HeaterControllerState{};
    st_.mode = HeaterMode::SAFE_OFF;
    st_.windowStartMs = nowMs;
    st_.windowOnMs = 0;
    st_.lastChamberMs = 0;
    st_.lastChamber_dC = TEMP_INVALID_DC;
    st_.slopeFilt_dC_per_s = 0;
}

bool HeaterController::sensorsValid_(const HeaterControllerInput &in) const {
    if (in.chamber_dC == TEMP_INVALID_DC) {
        return false;
    }
    // Hotspot may be invalid (per T13 Option B). We treat it as "not available", not invalid.
    return true;
}

void HeaterController::updateSlope_(const HeaterControllerInput &in) {
    if (in.chamber_dC == TEMP_INVALID_DC) {
        return;
    }

    if (st_.lastChamber_dC == TEMP_INVALID_DC || st_.lastChamberMs == 0) {
        st_.lastChamber_dC = in.chamber_dC;
        st_.lastChamberMs = in.nowMs;
        st_.slopeFilt_dC_per_s = 0;
        return;
    }

    const uint32_t dtMs = (in.nowMs >= st_.lastChamberMs) ? (in.nowMs - st_.lastChamberMs) : 0;
    if (dtMs < 200) { // ignore too-fast updates
        return;
    }

    const int32_t dT = (int32_t)in.chamber_dC - (int32_t)st_.lastChamber_dC;

    // raw slope in dC/s (scaled by 1000 for ms)
    // slope = dT / dt_s = dT * 1000 / dtMs
    const int32_t slopeRaw_dC_per_s = (dT * 1000) / (int32_t)dtMs;

    // IIR low-pass: filt = filt + alpha*(raw - filt)
    // alpha in [0..256]
    const int32_t alpha = (int32_t)cfg_.slopeLpfAlpha_256;
    st_.slopeFilt_dC_per_s = st_.slopeFilt_dC_per_s + ((alpha * (slopeRaw_dC_per_s - st_.slopeFilt_dC_per_s)) / 256);

    st_.lastChamber_dC = in.chamber_dC;
    st_.lastChamberMs = in.nowMs;
}

int16_t HeaterController::predictedChamber_(int16_t chamber_dC, int16_t slope_dC_per_s) const {
    // predicted = chamber + slope * tau
    const int32_t pred = (int32_t)chamber_dC + (int32_t)slope_dC_per_s * (int32_t)cfg_.predictTau_s;
    return clamp_i16(pred, (int16_t)-2000, (int16_t)2000);
}

uint8_t HeaterController::clampDuty_(int32_t dutyPct, uint8_t minPct, uint8_t maxPct) const {
    if (dutyPct < (int32_t)minPct) {
        return minPct;
    }
    if (dutyPct > (int32_t)maxPct) {
        return maxPct;
    }
    return (uint8_t)dutyPct;
}

HeaterControllerOutput HeaterController::runSafe_(const HeaterControllerInput &in, uint8_t reasons) {
    HeaterControllerOutput out;
    out.mode = HeaterMode::SAFE_OFF;
    out.dutyPercent = 0;
    out.reasons = reasons;
    out.chamberSlope_dC_per_s = (int16_t)st_.slopeFilt_dC_per_s;
    out.predictedChamber_dC = TEMP_INVALID_DC;

    st_.mode = HeaterMode::SAFE_OFF;
    st_.windowOnMs = 0;
    st_.windowStartMs = in.nowMs;
    return out;
}

HeaterControllerOutput HeaterController::runHeatUp_(const HeaterControllerInput &in) {
    HeaterControllerOutput out;
    out.mode = HeaterMode::HEAT_UP;
    out.reasons = 0;

    // full power, but capped by hotspot if available
    uint8_t duty = 100;

    if (in.hotspot_dC != TEMP_INVALID_DC) {
        const int32_t delta = (int32_t)in.hotspot_dC - (int32_t)in.chamber_dC;
        if (delta >= cfg_.hotspotDeltaMax_dC) {
            duty = 0;
            out.reasons |= ReasonBits::HOTSPOT_CAP;
        }
        if (in.hotspot_dC >= cfg_.hotspotAbsMax_dC) {
            duty = 0;
            out.reasons |= ReasonBits::HOTSPOT_CAP;
        }
    }

    out.dutyPercent = duty;
    out.chamberSlope_dC_per_s = (int16_t)st_.slopeFilt_dC_per_s;
    out.predictedChamber_dC = TEMP_INVALID_DC;

    st_.mode = HeaterMode::HEAT_UP;
    return out;
}

HeaterControllerOutput HeaterController::runApproach_(const HeaterControllerInput &in, int16_t slope_dC_per_s, int16_t pred_dC) {
    HeaterControllerOutput out;
    out.mode = HeaterMode::APPROACH;
    out.reasons = 0;

    const int16_t high_dC = (int16_t)(cfg_.target_dC + cfg_.maxOvershoot_dC);

    // predictive cutoff: if we're likely to exceed upper band, stop
    if (pred_dC >= high_dC) {
        out.dutyPercent = 0;
        out.reasons |= ReasonBits::PREDICT_CUTOFF;
    } else {
        // reduced power as we approach target
        // err is positive when below target
        const int32_t err = (int32_t)cfg_.target_dC - (int32_t)in.chamber_dC;

        // simple proportional mapping to 0..70% (no magic gains; very conservative)
        // 0 dC => 0%, 200 dC => ~70%
        int32_t duty = (err * 70) / 200;
        duty = clampDuty_(duty, 0, 70);

        // hotspot caps
        if (in.hotspot_dC != TEMP_INVALID_DC) {
            const int32_t delta = (int32_t)in.hotspot_dC - (int32_t)in.chamber_dC;
            if (delta >= cfg_.hotspotDeltaMax_dC || in.hotspot_dC >= cfg_.hotspotAbsMax_dC) {
                duty = 0;
                out.reasons |= ReasonBits::HOTSPOT_CAP;
            }
        }

        out.dutyPercent = (uint8_t)duty;
    }

    out.chamberSlope_dC_per_s = slope_dC_per_s;
    out.predictedChamber_dC = pred_dC;

    st_.mode = HeaterMode::APPROACH;
    return out;
}

uint8_t HeaterController::dutyHoldToWindowPct_(int16_t err_dC) const {
    // err positive => we are below target. Map err into duty.
    // Conservative mapping: 0..150 dC -> 0..maxDutyHoldPct
    if (err_dC <= 0) {
        return 0;
    }

    const int32_t duty = ((int32_t)err_dC * (int32_t)cfg_.maxDutyHoldPct) / 150;
    return clampDuty_(duty, cfg_.minDutyHoldPct, cfg_.maxDutyHoldPct);
}

bool HeaterController::windowShouldBeOn_(uint32_t nowMs) const {
    const uint32_t dt = nowMs - st_.windowStartMs;
    return dt < (uint32_t)st_.windowOnMs;
}

HeaterControllerOutput HeaterController::runHold_(const HeaterControllerInput &in, int16_t slope_dC_per_s, int16_t pred_dC) {
    HeaterControllerOutput out;
    out.mode = HeaterMode::HOLD;
    out.reasons = ReasonBits::IN_BAND;

    const int16_t low_dC = (int16_t)(cfg_.target_dC - cfg_.minUndershoot_dC);
    const int16_t high_dC = (int16_t)(cfg_.target_dC + cfg_.maxOvershoot_dC);

    // If we exceed upper bound, fully off until back in band.
    if (in.chamber_dC >= high_dC) {
        out.dutyPercent = 0;
        out.reasons |= ReasonBits::PREDICT_CUTOFF;
        st_.windowOnMs = 0;
    } else if (pred_dC >= high_dC) {
        // predictive off in hold as well
        out.dutyPercent = 0;
        out.reasons |= ReasonBits::PREDICT_CUTOFF;
        st_.windowOnMs = 0;
    } else {
        // windowed duty: compute err and set ON ms for current window
        const int16_t err_dC = (int16_t)((int32_t)cfg_.target_dC - (int32_t)in.chamber_dC);
        const uint8_t dutyPct = dutyHoldToWindowPct_(err_dC);

        // Manage window start
        if (st_.windowStartMs == 0) {
            st_.windowStartMs = in.nowMs;
        }

        const uint32_t dt = in.nowMs - st_.windowStartMs;
        if (dt >= cfg_.holdWindowMs) {
            // start next window
            st_.windowStartMs = in.nowMs;
        }

        st_.windowOnMs = (uint16_t)(((uint32_t)cfg_.holdWindowMs * (uint32_t)dutyPct) / 100u);
        out.dutyPercent = windowShouldBeOn_(in.nowMs) ? dutyPct : 0;
    }

    // hotspot caps
    if (in.hotspot_dC != TEMP_INVALID_DC) {
        const int32_t delta = (int32_t)in.hotspot_dC - (int32_t)in.chamber_dC;
        if (delta >= cfg_.hotspotDeltaMax_dC || in.hotspot_dC >= cfg_.hotspotAbsMax_dC) {
            out.dutyPercent = 0;
            out.reasons |= ReasonBits::HOTSPOT_CAP;
            st_.windowOnMs = 0;
        }
    }

    out.chamberSlope_dC_per_s = slope_dC_per_s;
    out.predictedChamber_dC = pred_dC;

    st_.mode = HeaterMode::HOLD;
    return out;
}

HeaterControllerOutput HeaterController::update(const HeaterControllerInput &in) {
    uint8_t reasons = 0;

    // Door hard gate
    if (!in.doorClosed) {
        reasons |= ReasonBits::DOOR_OPEN;
        return runSafe_(in, reasons);
    }

    // Hard abort on chamber/hotspot if available
    if (in.chamber_dC != TEMP_INVALID_DC && in.chamber_dC >= cfg_.abortTemp_dC) {
        reasons |= ReasonBits::ABORT_TEMP;
        return runSafe_(in, reasons);
    }
    if (in.hotspot_dC != TEMP_INVALID_DC && in.hotspot_dC >= cfg_.abortTemp_dC) {
        reasons |= ReasonBits::ABORT_TEMP;
        return runSafe_(in, reasons);
    }

    if (!sensorsValid_(in)) {
        reasons |= ReasonBits::INVALID_SENSOR;
        return runSafe_(in, reasons);
    }

    // update slope estimate
    updateSlope_(in);
    const int16_t slope_dC_per_s = (int16_t)st_.slopeFilt_dC_per_s;
    const int16_t pred_dC = predictedChamber_(in.chamber_dC, slope_dC_per_s);

    const int16_t low_dC = (int16_t)(cfg_.target_dC - cfg_.minUndershoot_dC);
    const int16_t high_dC = (int16_t)(cfg_.target_dC + cfg_.maxOvershoot_dC);

    // Mode transitions (minimal)
    // If well below band => HEAT_UP
    if (in.chamber_dC <= (int16_t)(low_dC - 50)) {
        st_.mode = HeaterMode::HEAT_UP;
        return runHeatUp_(in);
    }

    // If near band but below target => APPROACH
    if (in.chamber_dC < low_dC) {
        st_.mode = HeaterMode::APPROACH;
        return runApproach_(in, slope_dC_per_s, pred_dC);
    }

    // In band => HOLD
    if (in.chamber_dC >= low_dC && in.chamber_dC <= high_dC) {
        st_.mode = HeaterMode::HOLD;
        return runHold_(in, slope_dC_per_s, pred_dC);
    }

    // Above band => SAFE_OFF (until it cools)
    st_.mode = HeaterMode::SAFE_OFF;
    reasons |= ReasonBits::PREDICT_CUTOFF;
    return runSafe_(in, reasons);
}

} // namespace hcc
