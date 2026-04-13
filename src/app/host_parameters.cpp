#include "host_parameters.h"

#include <Preferences.h>
#include <cstring>

namespace {

static constexpr const char *kNvsNamespace = "host-params";
static constexpr const char *kBlobKey = "cfg";
static constexpr uint16_t kVersion = 3;

typedef struct HostParametersBlob {
    uint16_t version;
    HostParameters params;
} HostParametersBlob;

static HostParameters s_cached_params = {};
static bool s_initialized = false;

static bool validate_profile(const HostHeaterProfileParameters &profile) {
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

static bool validate_silica_pulse(const HostSilicaPulseParameters &pulse) {
    if (pulse.reheatSoakMs < 5000 || pulse.reheatSoakMs > 120000) {
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

static bool validate_params(const HostParameters &params) {
    for (uint8_t i = 0; i < HOST_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        if (params.shortcutPresetIds[i] >= kPresetCount) {
            return false;
        }
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        if (!validate_profile(params.heaterProfiles[i])) {
            return false;
        }
    }
    if (!validate_silica_pulse(params.silicaPulse)) {
        return false;
    }
    if (params.displayDimPercent < HOST_PARAMETER_DISPLAY_DIM_PERCENT_MIN ||
        params.displayDimPercent > HOST_PARAMETER_DISPLAY_DIM_PERCENT_MAX) {
        return false;
    }
    if (params.displayDimTimeoutMin > HOST_PARAMETER_DISPLAY_TIMEOUT_MIN_MAX) {
        return false;
    }
    return true;
}

} // namespace

void host_parameters_get_defaults(HostParameters *out) {
    if (!out) {
        return;
    }

    static constexpr uint16_t kDefaultShortcuts[HOST_PARAMETER_SHORTCUT_SLOT_COUNT] = {5, 4, 3, 6};
    static constexpr HostHeaterProfileParameters kDefaultProfiles[HOST_PARAMETER_HEATER_PROFILE_COUNT] = {
        {45, 15, 100, 40, 20},
        {60, 15, 100, 40, 20},
        {80, 15, 100, 40, 20},
        {100, 25, 100, 25, 30},
    };
    static constexpr HostSilicaPulseParameters kDefaultSilicaPulse = {
        25000,
        8000,
        30,
        10,
    };
    static constexpr uint8_t kDefaultDisplayDimPercent = 30;
    static constexpr uint8_t kDefaultDisplayDimTimeoutMin = 10;

    for (uint8_t i = 0; i < HOST_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        out->shortcutPresetIds[i] = kDefaultShortcuts[i];
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        out->heaterProfiles[i] = kDefaultProfiles[i];
    }
    out->silicaPulse = kDefaultSilicaPulse;
    out->displayDimPercent = kDefaultDisplayDimPercent;
    out->displayDimTimeoutMin = kDefaultDisplayDimTimeoutMin;
}

void host_parameters_init(void) {
    HostParameters defaults{};
    host_parameters_get_defaults(&defaults);
    s_cached_params = defaults;

    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, true)) {
        s_initialized = true;
        return;
    }

    HostParametersBlob blob{};
    const size_t read_size = prefs.getBytes(kBlobKey, &blob, sizeof(blob));
    prefs.end();

    if (read_size == sizeof(blob) &&
        blob.version == kVersion &&
        validate_params(blob.params)) {
        s_cached_params = blob.params;
    }

    s_initialized = true;
}

void host_parameters_get(HostParameters *out) {
    if (!out) {
        return;
    }
    if (!s_initialized) {
        host_parameters_init();
    }
    *out = s_cached_params;
}

const HostParameters *host_parameters_get_cached(void) {
    if (!s_initialized) {
        host_parameters_init();
    }
    return &s_cached_params;
}

bool host_parameters_save(const HostParameters *params) {
    if (!params || !validate_params(*params)) {
        return false;
    }

    HostParametersBlob blob{};
    blob.version = kVersion;
    blob.params = *params;

    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, false)) {
        return false;
    }

    const size_t written = prefs.putBytes(kBlobKey, &blob, sizeof(blob));
    prefs.end();

    if (written != sizeof(blob)) {
        return false;
    }

    s_cached_params = *params;
    s_initialized = true;
    return true;
}
