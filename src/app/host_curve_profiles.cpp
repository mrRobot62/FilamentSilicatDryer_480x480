#include "host_curve_profiles.h"

bool host_curve_profile_index_valid(uint8_t profileIndex) {
    return profileIndex < HOST_PARAMETER_HEATER_PROFILE_COUNT;
}

HostHeaterProfileParameters host_curve_default_heater_profile(uint8_t profileIndex) {
    static constexpr HostHeaterProfileParameters kDefaultProfiles[HOST_PARAMETER_HEATER_PROFILE_COUNT] = {
        {45, 15, 100, 40, 20},
        {60, 15, 100, 40, 20},
        {80, 15, 100, 40, 20},
        {100, 25, 100, 25, 30},
    };

    if (!host_curve_profile_index_valid(profileIndex)) {
        return kDefaultProfiles[0];
    }
    return kDefaultProfiles[profileIndex];
}

HostPulseCurveParameters host_curve_default_pulse_curve(uint8_t profileIndex) {
    static constexpr HostPulseCurveParameters kDefaultPulseCurves[HOST_PARAMETER_HEATER_PROFILE_COUNT] = {
        {30000, 6000, 30, 10},
        {30000, 5000, 30, 10},
        {30000, 6000, 20, 10},
        {25000, 8000, 30, 10},
    };

    if (!host_curve_profile_index_valid(profileIndex)) {
        return kDefaultPulseCurves[0];
    }
    return kDefaultPulseCurves[profileIndex];
}

bool host_curve_validate_heater_profile(const HostHeaterProfileParameters &profile) {
    if (profile.targetC < 30 || profile.targetC > 120) {
        return false;
    }
    if (profile.hysteresis_dC < 5 || profile.hysteresis_dC > 50) {
        return false;
    }
    if (profile.approachBand_dC < 10 || profile.approachBand_dC > 200) {
        return false;
    }
    if (profile.holdBand_dC < 5 || profile.holdBand_dC > 100) {
        return false;
    }
    if (profile.overshootCap_dC < 5 || profile.overshootCap_dC > 50) {
        return false;
    }
    return true;
}

bool host_curve_validate_pulse_curve(const HostPulseCurveParameters &pulse) {
    if (pulse.reheatSoakMs < 5000 || pulse.reheatSoakMs > 60000) {
        return false;
    }
    if (pulse.holdPulseMaxMs < 1000 || pulse.holdPulseMaxMs > 30000) {
        return false;
    }
    if (pulse.reheatEnableBelowTarget_dC < 5 || pulse.reheatEnableBelowTarget_dC > 100) {
        return false;
    }
    if (pulse.forceOffBeforeTarget_dC < 5 || pulse.forceOffBeforeTarget_dC > 50) {
        return false;
    }
    return true;
}
