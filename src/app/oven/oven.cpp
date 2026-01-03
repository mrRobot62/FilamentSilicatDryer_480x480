#include "oven.h"
#include <Arduino.h>

// Internal static state -----------------------------------
static bool waiting = false;
static OvenRuntimeState preWaitSnapshot = {};
static bool hasPreWaitSnapshot = false;

static OvenProfile currentProfile = {
    .durationMinutes = 60, // 1 hour default
    .targetTemperature = 45.0f,
    .filamentId = 0};

static OvenRuntimeState runtimeState = {
    .durationMinutes = 60,
    .secondsRemaining = 60 * 60, // 1 hour in seconds

    .tempCurrent = 25.0f,
    .tempTarget = 40.0f,

    .tempToleranceC = 3.0f,
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

    .running = false};

// ---- HostComm integration (T6) ---------------------------------------------
static HostComm *g_hostComm = nullptr;
static bool g_hasRealTelemetry = false;

static uint32_t g_lastStatusRequestMs = 0;

static inline bool mask_bit(uint16_t mask, OVEN_CONNECTOR c) {
    return (mask & static_cast<uint32_t>(c)) != 0u;
}

static inline uint16_t connector_u16(OVEN_CONNECTOR c) {
    return static_cast<uint16_t>(c);
}

static inline bool mask_has(uint16_t mask, OVEN_CONNECTOR c) {
    return (mask & connector_u16(c)) != 0u;
}

static inline uint16_t mask_set(uint16_t mask, OVEN_CONNECTOR c, bool on) {
    if (on) {
        return static_cast<uint16_t>(mask | connector_u16(c));
    }
    return static_cast<uint16_t>(mask & ~connector_u16(c));
}

static inline uint16_t mask_toggle(uint16_t mask, OVEN_CONNECTOR c) {
    return static_cast<uint16_t>(mask ^ connector_u16(c));
}

static inline uint16_t preserve_inputs(uint16_t mask) {
    // Preserve DOOR_ACTIVE as reported by the client (sensor input)
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

static void apply_remote_status_to_runtime(const ProtocolStatus &st) {

    //
    // TODO
    // anpassen - akutell wird die Temperatur einfach durch 4.0 geteilt (1/4 deg = 0.25 Auflösung )
    runtimeState.tempCurrent = static_cast<float>(st.tempRaw) / 4.0f;

    // Outputs mapping (P0..P6 are bit positions)
    // bit 0
    runtimeState.fan12v_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::FAN12V);
    runtimeState.fan230_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::FAN230V);
    runtimeState.fan230_slow_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::FAN230V_SLOW);
    runtimeState.lamp_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::LAMP);
    runtimeState.motor_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::SILICAT_MOTOR);
    // bit 5
    runtimeState.door_open = mask_bit(st.outputsMask, OVEN_CONNECTOR::DOOR_ACTIVE);
    // bit 6
    runtimeState.heater_on = mask_bit(st.outputsMask, OVEN_CONNECTOR::HEATER);

    g_hasRealTelemetry = true;
    g_remoteOutputsMask = st.outputsMask;

    g_lastStatusRxMs = millis();
    g_statusRxCount++;
}

// -------------- Basic API --------------

int oven_get_current_preset_index(void) {
    return runtimeState.filamentId;
}

const FilamentPreset *oven_get_preset(uint16_t index) {
    if (index >= kPresetCount) {
        return &kPresets[0]; // return CUSTOM as fallback
    }
    return &kPresets[index];
}

/**
 * Ofen-Initialisierung
 *
 * setzt default-Werte, initialisiert I2C Ports, ...
 */
void oven_init(void) {
    runtimeState.tempCurrent = 0.0f;

    // Ensure runtimeState.presetName + profile are consistent at boot
    oven_select_preset(OVEN_DEFAULT_PRESET_INDEX);

    OVEN_INFO("[OVEN] Init OK\n");
}

/**
 * OFEN - START
 *
 * wird aufgerufen um den Ofen "scharf" zu schalten. Sobald aufgerufen wird wird
 * die interne Variable "running" auf true gesetzt. Der Heizer = ON, FAN12VON, FAN230-SLOW=ON
 *
 * Der auf der PowerPlatine PN8034M wird im Betrieb sehr warm, daher die Dauerkühlung
 */
void oven_start(void) {
    if (runtimeState.running) {
        return;
    }

    runtimeState.running = true;

    // Sekunden nur beim Start neu setzen,
    // falls das Profil eine sinnvolle Dauer hat.
    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;

    // wenn die Heizung ON ist, muss der fan12V laufen als auch der 230V-Lüfter auf SLOW
    // der Fan12V muss laufen, weil das IC auf der PowerPlatine
    // runtimeState.heater_on = true;
    // runtimeState.fan12v_on = true;
    // runtimeState.fan230_slow_on = true;

    uint16_t m = g_remoteOutputsMask;

    // START policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);

    // mutual exclusion
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);

    // DOOR der Port wird auf high bezogen (5V), damit wird die Elektronik
    // aktiv und hardwaretechnisch werden einige Sicherheitsfeatures genutzt.
    //
    // Fällt der Port dann auf GND, wurde die Tür geöffnet
    m = mask_set(m, OVEN_CONNECTOR::DOOR_ACTIVE, true);

    comm_send_mask(m);

    OVEN_INFO("[OVEN] START\n");
}

/**
 *
 * OFEN - STOP
 *
 * setzt running=false, heater=off, fan230_on =false, 230_slow_on = false, motor=off
 *
 */
void oven_stop(void) {
    if (!runtimeState.running) {
        return;
    }

    runtimeState.running = false;

    // // Reset actuators
    // runtimeState.heater_on = false;
    // runtimeState.fan12v_on = false;
    // runtimeState.fan230_on = false;
    // runtimeState.fan230_slow_on = false;
    // runtimeState.motor_on = false;

    uint16_t m = g_remoteOutputsMask;

    // STOP policy
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);

    comm_send_mask(m);

    OVEN_INFO("[OVEN] STOP\n");
}

/**
 * Rückgabe des aktuellen Ofen-status
 */
bool oven_is_running(void) {
    return runtimeState.running;
}

// -------------- Profile --------------

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

    strncpy(runtimeState.presetName, p.name,
            sizeof(runtimeState.presetName) - 1);
    runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

    // SILICA behavior
    runtimeState.motor_on = p.rotaryOn;

    OVEN_INFO("[OVEN] Preset selected: ", runtimeState.presetName);
}

uint16_t oven_get_preset_count(void) {
    return kPresetCount;
}

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
}

// -------------- Runtime --------------
/** GETTER runtime-state */
void oven_get_runtime_state(OvenRuntimeState *out) {
    if (!out) {
        return;
    }
    *out = runtimeState;
}

/** OVEN-Tick, Sekunden-Tick, setzt den nächsten Tick */
void oven_tick(void) {
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < 1000) {
        return;
    }

    lastTick = now;

    // -------------------
    // Fake temperature
    // -------------------
    if (!g_hasRealTelemetry) {
        // if (runtimeState.heater_on) {
        //     // warm up slowly
        //     runtimeState.tempCurrent += 0.5f;
        //     if (runtimeState.tempCurrent > runtimeState.tempTarget + runtimeState.tempToleranceC) {
        //         runtimeState.heater_on = false;
        //     }
        // } else {
        //     if (oven_is_running()) {
        //         if (runtimeState.tempCurrent < runtimeState.tempTarget - runtimeState.tempToleranceC) {
        //             runtimeState.heater_on = true;
        //             runtimeState.tempCurrent += 0.3f;
        //         } else {
        //             runtimeState.heater_on = false;
        //             runtimeState.tempCurrent -= 0.1f;
        //         }
        //     } else {
        //         // cool down
        //         runtimeState.tempCurrent -= 0.2f;
        //         if (runtimeState.tempCurrent < 25.0f) {
        //             runtimeState.tempCurrent = 25.0f;
        //         }
        //     }
        // }
    }

    // -------------------
    // Countdown
    // -------------------
    if (runtimeState.running) {
        if (runtimeState.durationMinutes > 0) {
            if (runtimeState.secondsRemaining > 0) {
                runtimeState.secondsRemaining--;
            } else {
                oven_stop();
            }
        }
    }

    // --- Communication diagnostics (host-side) ---
    now = millis();
    const uint32_t age = (g_lastStatusRxMs == 0) ? 0xFFFFFFFFu : (now - g_lastStatusRxMs);

    runtimeState.lastStatusAgeMs = age;
    runtimeState.commAlive = (g_lastStatusRxMs != 0) && (age <= kCommAliveTimeoutMs);
    runtimeState.statusRxCount = g_statusRxCount;
    runtimeState.commErrorCount = g_commErrorCount;

    // auto-controls could be added here later
}

// -------------- Manual Overrides (UI) --------------
//
// nur bestimmte Aktuatoren können über den Anwender
// aus Sicherheitsgründen geschaltet werden
//
// FAN230 ON/OFF, MOTOR ON/OFF, LAMP ON/OFF
//
// *****************************************************
// Fan12V, Tür, Heater dürfen grundsätzlich vom Anwender
// nicht geschaltet werden.
// *****************************************************
// ---------------------------------------------------
/** FAN230 manuell togglen ON/OFF */
void oven_fan230_toggle_manual(void) {
    // OVEN_INFO("[OVEN] oven_fan230_toggle_manual called\n");
    if (!runtimeState.fan230_manual_allowed) {
        return;
    }

    // OVEN_INFO("[OVEN] oven_fan230_toggle_manual proceeding\n");
    if (oven_is_running()) {
        return;
    }

    // runtimeState.fan230_on = !runtimeState.fan230_on;
    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::FAN230V);

    m = mask_set(m, OVEN_CONNECTOR::FAN230V, newState);
    if (newState) {
        // mutual exclusion
        m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, false);
    }

    comm_send_mask(m);
    OVEN_INFO("[OVEN] Fan230 toggled: ", (runtimeState.fan230_on ? "ON" : "OFF"));
}

/** Silicat-Motor toggeln ON/OFF */
void oven_command_toggle_motor_manual(void) {
    if (!runtimeState.motor_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::SILICAT_MOTOR);

    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, newState);
    comm_send_mask(m);

    OVEN_INFO("[OVEN] Motor requested: ", (newState ? "ON" : "OFF"));
}

/** Lampe toggeln ON/OFF */
void oven_lamp_toggle_manual(void) {
    if (!runtimeState.lamp_manual_allowed) {
        return;
    }

    uint16_t m = g_remoteOutputsMask;
    const bool newState = !mask_has(m, OVEN_CONNECTOR::LAMP);

    m = mask_set(m, OVEN_CONNECTOR::LAMP, newState);
    comm_send_mask(m);

    OVEN_INFO("[OVEN] Lamp requested: ", (newState ? "ON" : "OFF"));
}

bool oven_is_waiting(void) {
    return waiting;
}

void oven_pause_wait(void) {
    // Only meaningful if currently running and not already waiting
    if (!runtimeState.running || waiting) {
        OVEN_WARN("[OVEN] (oven_pause_wait) runtimeState.running=%d || waiting=%d\n", runtimeState.running, waiting);
        return;
    }

    // Snapshot current actuator/runtime state
    preWaitSnapshot = runtimeState;
    hasPreWaitSnapshot = true;

    // Enter WAIT: stop countdown progression
    runtimeState.running = false;
    waiting = true;

    // Safety-first WAIT outputs:
    // Heater OFF, Motor OFF
    // runtimeState.heater_on = false;
    // runtimeState.motor_on = false;

    // // Fans + lamp behavior in WAIT (as you defined)
    // runtimeState.fan12v_on = true;
    // runtimeState.fan230_on = false;
    // runtimeState.fan230_slow_on = true;
    // runtimeState.lamp_on = true;

    uint16_t m = g_remoteOutputsMask;

    // WAIT policy (safety-first)
    m = mask_set(m, OVEN_CONNECTOR::HEATER, false);
    m = mask_set(m, OVEN_CONNECTOR::SILICAT_MOTOR, false);

    // Cooling / visibility behavior during WAIT
    m = mask_set(m, OVEN_CONNECTOR::FAN12V, true);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V, false);
    m = mask_set(m, OVEN_CONNECTOR::FAN230V_SLOW, true);
    m = mask_set(m, OVEN_CONNECTOR::LAMP, true);

    comm_send_mask(m);

    OVEN_INFO("[OVEN] WAIT (paused)\n");
}

bool oven_resume_from_wait(void) {
    if (!waiting) {
        OVEN_WARN("[OVEN] (oven_resume_from_wait) waiting=%d\n", waiting);
        return false;
    }

    // Safety rule: never resume when door open (telemetry)
    if (runtimeState.door_open) {
        OVEN_WARN("[OVEN] (oven_resume_from_wait) RESUME blocked: door open\n");
        return false;
    }

    // Restore host/runtime fields (do NOT restore actuator bools!)
    if (hasPreWaitSnapshot) {
        // Keep secondsRemaining as-is (it was frozen during WAIT)
        const uint32_t keepSeconds = runtimeState.secondsRemaining;

        // Restore only host-related fields that the UI needs
        runtimeState.durationMinutes = preWaitSnapshot.durationMinutes;
        runtimeState.tempTarget = preWaitSnapshot.tempTarget;
        runtimeState.filamentId = preWaitSnapshot.filamentId;
        runtimeState.rotaryOn = preWaitSnapshot.rotaryOn;

        std::strncpy(runtimeState.presetName, preWaitSnapshot.presetName, sizeof(runtimeState.presetName) - 1);
        runtimeState.presetName[sizeof(runtimeState.presetName) - 1] = '\0';

        runtimeState.secondsRemaining = keepSeconds;
    }

    // Host-state
    runtimeState.running = true;
    waiting = false;

    // Restore the pre-WAIT command mask
    comm_send_mask(g_preWaitCommandMask);

    OVEN_INFO("[OVEN] (oven_resume_from_wait) RESUME from WAIT\n");
    return true;
}

// // Safety rule: never resume when door open
// if (runtimeState.door_open) {
//     OVEN_WARN("[OVEN] (oven_resume_from_wait) RESUME blocked: door open\n");
//     return false;
// }

// // Restore snapshot if available
// if (hasPreWaitSnapshot) {
//     bool keepDoor = runtimeState.door_open;               // should be false here, but keep it safe
//     uint32_t keepSeconds = runtimeState.secondsRemaining; // keep remaining time

//     runtimeState = preWaitSnapshot;

//     runtimeState.door_open = keepDoor;
//     runtimeState.secondsRemaining = keepSeconds;
// }

// // Enforce safety again (even after restore)
// if (runtimeState.door_open) {
//     runtimeState.heater_on = false;
//     runtimeState.motor_on = false;
// }

//     runtimeState.running = true;
//     waiting = false;

//     OVEN_INFO("[OVEN] (oven_resume_from_wait) RESUME from WAIT\n");
//     return true;
// }

void oven_set_runtime_duration_minutes(uint16_t duration_min) {
    if (duration_min == 0) {
        return;
    }
    currentProfile.durationMinutes = duration_min;

    runtimeState.durationMinutes = duration_min;
    runtimeState.secondsRemaining = duration_min * 60;

    OVEN_INFO("[OVEN] Runtime duration set to %d minutes\n", duration_min);
}

void oven_set_runtime_temp_target(uint16_t temp_c) {
    currentProfile.targetTemperature = static_cast<float>(temp_c);
    runtimeState.tempTarget = static_cast<float>(temp_c);
    OVEN_INFO("[OVEN] Runtime target temperature set to %d °C\n", temp_c);
}

void oven_set_runtime_actuator_fan230(bool on) {
    runtimeState.fan230_on = on;
    if (on) {
        runtimeState.fan230_slow_on = false; // optional mutual exclusion
    }
    OVEN_INFO("[OVEN] Runtime actuator fan230 set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_fan230_slow(bool on) {
    runtimeState.fan230_slow_on = on;
    if (on) {
        runtimeState.fan230_on = false; // optional mutual exclusion
    }
    OVEN_INFO("[OVEN] Runtime actuator fan230_slow set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_heater(bool on) {
    runtimeState.heater_on = on;
    OVEN_INFO("[OVEN] Runtime actuator heater set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_motor(bool on) {
    runtimeState.motor_on = on;
    OVEN_INFO("[OVEN] Runtime actuator motor set to %s\n", on ? "ON" : "OFF");
}

void oven_set_runtime_actuator_lamp(bool on) {
    runtimeState.lamp_on = on;
    OVEN_INFO("[OVEN] Runtime actuator lamp set to %s\n", on ? "ON" : "OFF");
}
void oven_comm_init(HardwareSerial &serial, uint32_t baudrate, uint8_t rx, uint8_t tx) {
    static HostComm comm(serial);
    g_hostComm = &comm;

    g_hostComm->begin(baudrate, rx, tx);
    g_hasRealTelemetry = false;
    g_lastStatusRequestMs = 0;

    OVEN_INFO("[OVEN] HostComm init OK\n");
}

void oven_comm_poll(void) {
    if (!g_hostComm) {
        return;
    }

    // Always read UART non-blocking
    g_hostComm->loop();

    // Periodically request STATUS
    const uint32_t now = millis();
    if (now - g_lastStatusRequestMs >= kStatusPollIntervalMs) {
        g_lastStatusRequestMs = now;
        g_hostComm->requestStatus();
    }

    // Apply new telemetry
    if (g_hostComm->hasNewStatus()) {
        apply_remote_status_to_runtime(g_hostComm->getRemoteStatus());
        g_hostComm->clearNewStatusFlag();
    }

    if (g_hostComm->hasCommError()) {
        OVEN_WARN("[OVEN] HostComm parse/protocol error\n");
        g_hostComm->clearCommErrorFlag();
    }

    if (g_hostComm->hasCommError()) {
        g_commErrorCount++;
        OVEN_WARN("[OVEN] HostComm parse/protocol error\n");
        g_hostComm->clearCommErrorFlag();
    }
}

// ---------------------------------------------------
// END OF FILE