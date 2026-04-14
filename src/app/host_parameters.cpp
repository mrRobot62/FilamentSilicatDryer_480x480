#include "oven.h"
#include "host_curve_profiles.h"
#include "host_parameters.h"

#include <Preferences.h>
#include <cstring>

namespace {

static constexpr const char *kNvsNamespace = "host-params";
static constexpr const char *kBlobKey = "cfg";
static constexpr uint16_t kVersion = 4;

typedef struct HostParametersBlob {
    uint16_t version;
    HostParameters params;
} HostParametersBlob;

static HostParameters s_cached_params = {};
static bool s_initialized = false;

static bool validate_params(const HostParameters &params) {
    for (uint8_t i = 0; i < HOST_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        if (params.shortcutPresetIds[i] >= kPresetCount) {
            return false;
        }
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        if (!host_curve_validate_heater_profile(params.heaterProfiles[i])) {
            return false;
        }
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        if (!host_curve_validate_pulse_curve(params.pulseCurves[i])) {
            return false;
        }
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
    static constexpr uint8_t kDefaultDisplayDimPercent = 30;
    static constexpr uint8_t kDefaultDisplayDimTimeoutMin = 10;

    for (uint8_t i = 0; i < HOST_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        out->shortcutPresetIds[i] = kDefaultShortcuts[i];
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        out->heaterProfiles[i] = host_curve_default_heater_profile(i);
    }
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        out->pulseCurves[i] = host_curve_default_pulse_curve(i);
    }
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
