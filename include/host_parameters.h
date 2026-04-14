#pragma once

#include <stdbool.h>
#include <stdint.h>

static constexpr uint8_t HOST_PARAMETER_SHORTCUT_SLOT_COUNT = 4;
static constexpr uint8_t HOST_PARAMETER_HEATER_PROFILE_COUNT = 4;
static constexpr uint8_t HOST_PARAMETER_DISPLAY_DIM_PERCENT_MIN = 5;
static constexpr uint8_t HOST_PARAMETER_DISPLAY_DIM_PERCENT_MAX = 100;
static constexpr uint8_t HOST_PARAMETER_DISPLAY_TIMEOUT_MIN_MAX = 30;

typedef struct HostHeaterProfileParameters {
    int16_t targetC;
    int16_t hysteresis_dC;
    int16_t approachBand_dC;
    int16_t holdBand_dC;
    int16_t overshootCap_dC;
} HostHeaterProfileParameters;

typedef struct HostPulseCurveParameters {
    uint16_t reheatSoakMs;
    uint16_t holdPulseMaxMs;
    int16_t reheatEnableBelowTarget_dC;
    int16_t forceOffBeforeTarget_dC;
} HostPulseCurveParameters;

typedef struct HostParameters {
    uint16_t shortcutPresetIds[HOST_PARAMETER_SHORTCUT_SLOT_COUNT];
    HostHeaterProfileParameters heaterProfiles[HOST_PARAMETER_HEATER_PROFILE_COUNT];
    HostPulseCurveParameters pulseCurves[HOST_PARAMETER_HEATER_PROFILE_COUNT];
    uint8_t displayDimPercent;
    uint8_t displayDimTimeoutMin;
} HostParameters;

void host_parameters_init(void);
void host_parameters_get_defaults(HostParameters *out);
void host_parameters_get(HostParameters *out);
const HostParameters *host_parameters_get_cached(void);
bool host_parameters_save(const HostParameters *params);
