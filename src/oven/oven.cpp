#include "oven.h"
#include <Arduino.h>

// Internal static state -----------------------------------

static OvenProfile currentProfile = {
    .durationMinutes = 60, // 1 hour default
    .targetTemperature = 70.0f,
    .filamentId = 0};

static OvenRuntimeState runtimeState = {
    .durationMinutes = 60,
    .secondsRemaining = 60 * 60, // 1 hour in seconds

    .tempCurrent = 25.0f,
    .tempTarget = 70.0f,
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

void oven_init(void)
{
    // Later: configure GPIO, sensors, PWM, I2C, watchdog, ...
    runtimeState.tempCurrent = 25.0f;

    Serial.println(F("[OVEN] Init OK"));
}
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

    runtimeState.heater_on = true;
    runtimeState.fan12v_on = true;

    Serial.println(F("[OVEN] START"));
}

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

bool oven_is_running(void)
{
    return runtimeState.running;
}

// -------------- Profile --------------

void oven_set_profile(const OvenProfile *p)
{
    if (!p)
        return;

    currentProfile = *p;

    runtimeState.filamentId = p->filamentId;
    runtimeState.durationMinutes = p->durationMinutes;
    runtimeState.tempTarget = p->targetTemperature;

    // Wenn der Ofen aktuell nicht läuft, Countdown direkt vorbereiten
    if (!runtimeState.running)
    {
        runtimeState.secondsRemaining = runtimeState.durationMinutes * 60;
    }

    Serial.println(F("[OVEN] Profile updated"));
}

void oven_get_profile(OvenProfile *pOut)
{
    if (!pOut)
        return;
    *pOut = currentProfile;
}

// -------------- Runtime --------------

void oven_get_runtime_state(OvenRuntimeState *out)
{
    if (!out)
        return;
    *out = runtimeState;
}

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
        runtimeState.tempCurrent += 0.3f;
        if (runtimeState.tempCurrent > runtimeState.tempTarget)
            runtimeState.heater_on = false;
    }
    else
    {
        // cool down
        runtimeState.tempCurrent -= 0.2f;
        if (runtimeState.tempCurrent < 25.0f)
            runtimeState.tempCurrent = 25.0f;
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

void oven_command_toggle_fan230_manual(void)
{
    if (!runtimeState.fan230_manual_allowed)
        return;

    runtimeState.fan230_on = !runtimeState.fan230_on;

    Serial.print(F("[OVEN] Fan230 toggled: "));
    Serial.println(runtimeState.fan230_on ? "ON" : "OFF");
}

void oven_command_toggle_motor_manual(void)
{
    if (!runtimeState.motor_manual_allowed)
        return;

    runtimeState.motor_on = !runtimeState.motor_on;

    Serial.print(F("[OVEN] Motor toggled: "));
    Serial.println(runtimeState.motor_on ? "ON" : "OFF");
}

void oven_command_toggle_lamp_manual(void)
{
    if (!runtimeState.lamp_manual_allowed)
        return;

    runtimeState.lamp_on = !runtimeState.lamp_on;

    Serial.print(F("[OVEN] Lamp toggled: "));
    Serial.println(runtimeState.lamp_on ? "ON" : "OFF");
}