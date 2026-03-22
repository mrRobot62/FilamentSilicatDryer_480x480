#include "client/heater_io.h"

#include <Arduino.h>

#include "log_client.h"
#include "pins_client.h"

namespace heater_io {

static constexpr int kHeaterSafeLevel = LOW;
static bool g_running = false;

static uint32_t duty_from_percent(uint8_t percent) {
    const uint32_t maxDuty = (1u << kPwmResBits) - 1u;
    if (percent == 0) {
        return 0;
    }
    if (percent >= 100) {
        return maxDuty;
    }
    return (maxDuty * (uint32_t)percent + 50u) / 100u;
}

void init_off() {
    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, kHeaterSafeLevel);
}

bool set_enabled(bool enabled, uint8_t dutyPercent) {
    const uint32_t duty = duty_from_percent(dutyPercent);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (enabled) {
        pinMode(OVEN_HEATER, OUTPUT);
        digitalWrite(OVEN_HEATER, kHeaterSafeLevel);

        (void)ledcDetach((uint8_t)OVEN_HEATER);
        g_running = false;

        const bool ok = ledcAttach((uint8_t)OVEN_HEATER,
                                   (uint32_t)kPwmFreqHz,
                                   (uint8_t)kPwmResBits);

        if (!ok) {
            CLIENT_ERR("[HEATER] ledcAttach FAILED (GPIO=%d, Freq=%luHz, Res=%dbit)\n",
                       OVEN_HEATER,
                       (unsigned long)kPwmFreqHz,
                       (int)kPwmResBits);
            pinMode(OVEN_HEATER, OUTPUT);
            digitalWrite(OVEN_HEATER, kHeaterSafeLevel);
            g_running = false;
            return false;
        }

        g_running = true;

        CLIENT_INFO("[HEATER] PWM attached. GPIO=%d, Freq=%luHz, Duty=%u%%\n",
                    OVEN_HEATER,
                    (unsigned long)kPwmFreqHz,
                    (unsigned)dutyPercent);

        ledcWrite((uint8_t)OVEN_HEATER, 0);
        delay(2);
        ledcWrite((uint8_t)OVEN_HEATER, duty);
        delay(2);
        ledcWrite((uint8_t)OVEN_HEATER, duty);
        return true;
    }

    if (g_running) {
        ledcWrite((uint8_t)OVEN_HEATER, 0);
        ledcDetach((uint8_t)OVEN_HEATER);
        g_running = false;
        CLIENT_INFO("[HEATER] PWM detached (stopped)\n");
    }

    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, kHeaterSafeLevel);
    return true;
#else
    static constexpr uint8_t kHeaterPwmChannel = 0;

    if (enabled) {
        if (!g_running) {
            ledcSetup(kHeaterPwmChannel, kPwmFreqHz, kPwmResBits);
            ledcAttachPin(OVEN_HEATER, kHeaterPwmChannel);
            g_running = true;
        }

        ledcWrite(kHeaterPwmChannel, 0);
        delay(2);
        ledcWrite(kHeaterPwmChannel, duty);
        delay(2);
        ledcWrite(kHeaterPwmChannel, duty);
        return true;
    }

    if (g_running) {
        ledcWrite(kHeaterPwmChannel, 0);
        ledcDetachPin(OVEN_HEATER);
        g_running = false;
        CLIENT_INFO("[HEATER] PWM detached (stopped)\n");
    }

    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, kHeaterSafeLevel);
    return true;
#endif
}

bool is_running() {
    return g_running;
}

} // namespace heater_io
