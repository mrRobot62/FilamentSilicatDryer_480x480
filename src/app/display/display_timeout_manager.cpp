#include "display/display_timeout_manager.h"

#include <Arduino.h>

#include "display/display_backlight.h"
#include "display/display_dimmer.h"
#include "host_parameters.h"

namespace {

struct DisplayTimeoutState {
    uint8_t dim_percent = 30;
    uint8_t timeout_min = 10;
    uint32_t last_activity_ms = 0;
    bool dim_active = false;
    bool initialized = false;

    OvenMode mode = OvenMode::STOPPED;
    bool comm_alive = true;
    bool link_synced = true;
    bool door_open = false;
    bool host_overtemp = false;
    bool safety_cutoff = false;
};

static DisplayTimeoutState g_state = {};

static void brighten_and_restart_timeout(uint32_t now_ms) {
    g_state.last_activity_ms = now_ms;
    g_state.dim_active = false;
    display_backlight_set_percent(100);
    display_dimmer_set_brightness_percent(100);
}

static bool runtime_state_changed(const OvenRuntimeState &state) {
    return state.mode != g_state.mode ||
           state.commAlive != g_state.comm_alive ||
           state.linkSynced != g_state.link_synced ||
           state.door_open != g_state.door_open ||
           state.hostOvertempActive != g_state.host_overtemp ||
           state.safetyCutoffActive != g_state.safety_cutoff;
}

static void copy_runtime_markers(const OvenRuntimeState &state) {
    g_state.mode = state.mode;
    g_state.comm_alive = state.commAlive;
    g_state.link_synced = state.linkSynced;
    g_state.door_open = state.door_open;
    g_state.host_overtemp = state.hostOvertempActive;
    g_state.safety_cutoff = state.safetyCutoffActive;
}

} // namespace

void display_timeout_reload_from_host_parameters(void) {
    const HostParameters *params = host_parameters_get_cached();
    if (!params) {
        return;
    }

    g_state.dim_percent = params->displayDimPercent;
    g_state.timeout_min = params->displayDimTimeoutMin;
}

void display_timeout_init(void) {
    display_backlight_init();
    display_timeout_reload_from_host_parameters();
    g_state.last_activity_ms = millis();
    g_state.dim_active = false;
    g_state.initialized = true;
    display_backlight_set_percent(100);
}

bool display_timeout_consume_wake_touch(void) {
    if (!g_state.initialized || !g_state.dim_active) {
        return false;
    }

    brighten_and_restart_timeout(millis());
    return true;
}

void display_timeout_note_user_activity(void) {
    if (!g_state.initialized) {
        return;
    }
    brighten_and_restart_timeout(millis());
}

void display_timeout_note_runtime_state(const OvenRuntimeState *state) {
    if (!g_state.initialized || !state) {
        return;
    }

    if (runtime_state_changed(*state)) {
        brighten_and_restart_timeout(millis());
        copy_runtime_markers(*state);
        return;
    }

    copy_runtime_markers(*state);
}

void display_timeout_tick(uint32_t now_ms) {
    if (!g_state.initialized) {
        return;
    }

    if (g_state.timeout_min == 0) {
        if (g_state.dim_active || display_dimmer_get_brightness_percent() != 100) {
            g_state.dim_active = false;
            display_backlight_set_percent(100);
            display_dimmer_set_brightness_percent(100);
        }
        return;
    }

    const uint32_t timeout_ms = static_cast<uint32_t>(g_state.timeout_min) * 60UL * 1000UL;
    const bool should_dim = (now_ms - g_state.last_activity_ms) >= timeout_ms;

    if (should_dim && !g_state.dim_active) {
        g_state.dim_active = true;
        display_backlight_set_percent(100);
        display_dimmer_set_brightness_percent(g_state.dim_percent);
        return;
    }

    if (!should_dim && g_state.dim_active) {
        g_state.dim_active = false;
        display_backlight_set_percent(100);
        display_dimmer_set_brightness_percent(100);
    }
}
