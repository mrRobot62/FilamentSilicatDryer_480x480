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

// -------------- Basic API --------------

int oven_get_current_preset_index(void)
{
    return runtimeState.filamentId;
}

const FilamentPreset *oven_get_preset(uint16_t index)
{
    if (index >= kPresetCount)
    {
        return &kPresets[0]; // return CUSTOM as fallback
    }
    return &kPresets[index];
}

/**
 * Ofen-Initialisierung
 *
 * setzt default-Werte, initialisiert I2C Ports, ...
 */
void oven_init(void)
{
    runtimeState.tempCurrent = 0.0f;

    // Ensure runtimeState.presetName + profile are consistent at boot
    oven_select_preset(OVEN_DEFAULT_PRESET_INDEX);

    INFO("[OVEN] Init OK\n");
}

/**
 * OFEN - START
 *
 * wird aufgerufen um den Ofen "scharf" zu schalten. Sobald aufgerufen wird wird
 * die interne Variable "running" auf true gesetzt. Der Heizer = ON, FAN12VON, FAN230-SLOW=ON
 *
 * Der auf der PowerPlatine PN8034M wird im Betrieb sehr warm, daher die Dauerkühlung
 */
void oven_start(void)
{
    if (runtimeState.running)
        return;

    runtimeState.running = true;

    // Sekunden nur beim Start neu setzen,
    // falls das Profil eine sinnvolle Dauer hat.
    runtimeState.durationMinutes = currentProfile.durationMinutes;
    runtimeState.secondsRemaining = currentProfile.durationMinutes * 60;
    runtimeState.tempTarget = currentProfile.targetTemperature;

    // wenn die Heizung ON ist, muss der fan12V laufen als auch der 230V-Lüfter auf SLOW
    // der Fan12V muss laufen, weil das IC auf der PowerPlatine
    runtimeState.heater_on = true;
    runtimeState.fan12v_on = true;
    runtimeState.fan230_slow_on = true;

    Serial.println(F("[OVEN] START"));
}

/**
 *
 * OFEN - STOP
 *
 * setzt running=false, heater=off, fan230_on =false, 230_slow_on = false, motor=off
 *
 */
void oven_stop(void)
{
    if (!runtimeState.running)
        return;

    runtimeState.running = false;

    // Reset actuators
    runtimeState.heater_on = false;
    runtimeState.fan12v_on = false;
    runtimeState.fan230_on = false;
    runtimeState.fan230_slow_on = false;
    runtimeState.motor_on = false;

    Serial.println(F("[OVEN] STOP"));
}

/**
 * Rückgabe des aktuellen Ofen-status
 */
bool oven_is_running(void)
{
    return runtimeState.running;
}

// -------------- Profile --------------

void oven_select_preset(uint16_t index)
{
    if (index >= kPresetCount)
        return;

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

    Serial.print(F("[OVEN] Preset selected: "));
    Serial.println(runtimeState.presetName);
}

uint16_t oven_get_preset_count(void)
{
    return kPresetCount;
}

void oven_get_preset_name(uint16_t index, char *out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    if (index >= kPresetCount)
    {
        out[0] = '\0';
        return;
    }

    std::strncpy(out, kPresets[index].name, out_len - 1);
    out[out_len - 1] = '\0';
}

// -------------- Runtime --------------
/** GETTER runtime-state */
void oven_get_runtime_state(OvenRuntimeState *out)
{
    if (!out)
        return;
    *out = runtimeState;
}

/** OVEN-Tick, Sekunden-Tick, setzt den nächsten Tick */
void oven_tick(void)
{
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick < 1000)
        return;

    lastTick = now;

    // -------------------
    // Fake temperature
    // -------------------
    if (runtimeState.heater_on)
    {
        // warm up slowly
        runtimeState.tempCurrent += 0.5f;
        if (runtimeState.tempCurrent > runtimeState.tempTarget + runtimeState.tempToleranceC)
            runtimeState.heater_on = false;
    }
    else
    {
        if (oven_is_running())
        {
            if (runtimeState.tempCurrent < runtimeState.tempTarget - runtimeState.tempToleranceC)
            {
                runtimeState.heater_on = true;
                runtimeState.tempCurrent += 0.3f;
            }
            else
            {
                runtimeState.heater_on = false;
                runtimeState.tempCurrent -= 0.1f;
            }
        }
        else
        {
            // cool down
            runtimeState.tempCurrent -= 0.2f;
            if (runtimeState.tempCurrent < 25.0f)
                runtimeState.tempCurrent = 25.0f;
        }
    }

    // -------------------
    // Countdown
    // -------------------
    if (runtimeState.running)
    {
        if (runtimeState.durationMinutes > 0)
        {
            if (runtimeState.secondsRemaining > 0)
            {
                runtimeState.secondsRemaining--;
            }
            else
            {
                oven_stop();
            }
        }
    }

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
void oven_fan230_toggle_manual(void)
{
    // OVEN_INFO("[OVEN] oven_fan230_toggle_manual called\n");
    if (!runtimeState.fan230_manual_allowed)
        return;

    // OVEN_INFO("[OVEN] oven_fan230_toggle_manual proceeding\n");
    if (oven_is_running())
        return;

    runtimeState.fan230_on = !runtimeState.fan230_on;

    OVEN_INFO("[OVEN] Fan230 toggled: ", (runtimeState.fan230_on ? "ON" : "OFF"));
}

/** Silicat-Motor toggeln ON/OFF */
void oven_command_toggle_motor_manual(void)
{
    if (!runtimeState.motor_manual_allowed)
        return;

    runtimeState.motor_on = !runtimeState.motor_on;

    Serial.print(F("[OVEN] Motor toggled: "));
    Serial.println(runtimeState.motor_on ? "ON" : "OFF");
}

/** Lampe toggeln ON/OFF */
void oven_lamp_toggle_manual(void)
{
    if (!runtimeState.lamp_manual_allowed)
        return;

    runtimeState.lamp_on = !runtimeState.lamp_on;

    Serial.print(F("[OVEN] Lamp toggled: "));
    Serial.println(runtimeState.lamp_on ? "ON" : "OFF");
}

bool oven_is_waiting(void)
{
    return waiting;
}

void oven_pause_wait(void)
{
    // Only meaningful if currently running and not already waiting
    if (!runtimeState.running || waiting)
        return;

    // Snapshot current actuator/runtime state
    preWaitSnapshot = runtimeState;
    hasPreWaitSnapshot = true;

    // Enter WAIT: stop countdown progression
    runtimeState.running = false;
    waiting = true;

    // Safety-first WAIT outputs:
    // Heater OFF, Motor OFF
    runtimeState.heater_on = false;
    runtimeState.motor_on = false;

    // Fans + lamp behavior in WAIT (as you defined)
    runtimeState.fan12v_on = true;
    runtimeState.fan230_on = false;
    runtimeState.fan230_slow_on = true;
    runtimeState.lamp_on = true;

    Serial.println(F("[OVEN] WAIT (paused)"));
}

bool oven_resume_from_wait(void)
{
    if (!waiting)
        return false;

    // Safety rule: never resume when door open
    if (runtimeState.door_open)
    {
        Serial.println(F("[OVEN] RESUME blocked: door open"));
        return false;
    }

    // Restore snapshot if available
    if (hasPreWaitSnapshot)
    {
        bool keepDoor = runtimeState.door_open;               // should be false here, but keep it safe
        uint32_t keepSeconds = runtimeState.secondsRemaining; // keep remaining time

        runtimeState = preWaitSnapshot;

        runtimeState.door_open = keepDoor;
        runtimeState.secondsRemaining = keepSeconds;
    }

    // Enforce safety again (even after restore)
    if (runtimeState.door_open)
    {
        runtimeState.heater_on = false;
        runtimeState.motor_on = false;
    }

    runtimeState.running = true;
    waiting = false;

    Serial.println(F("[OVEN] RESUME from WAIT"));
    return true;
}

void oven_set_runtime_duration_minutes(uint16_t duration_min)
{
    if (duration_min == 0)
        return;

    runtimeState.durationMinutes = duration_min;
    runtimeState.secondsRemaining = duration_min * 60;

    Serial.print(F("[OVEN] Runtime duration set to "));
    Serial.print(duration_min);
    Serial.println(F(" minutes"));
}

void oven_set_runtime_temp_target(uint16_t temp_c)
{
    runtimeState.tempTarget = static_cast<float>(temp_c);

    Serial.print(F("[OVEN] Runtime target temperature set to "));
    Serial.print(temp_c);
    Serial.println(F(" °C"));
}

// ---------------------------------------------------
// END OF FILE