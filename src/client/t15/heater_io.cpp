#include "T15/heater_io.h"

#include <Arduino.h>

#include "log_core.h"
#include "pins_client.h"
#include "T15/t15_log_tags.h"

namespace t15_heater
{
static constexpr uint32_t HEATER_PWM_FREQ_HZ = 4000;
static constexpr uint8_t HEATER_PWM_RES_BITS = 10;
static constexpr int HEATER_SAFE_LEVEL = LOW;

static bool g_heaterPwmRunning = false;

static inline uint32_t heaterDutyFromPercent(uint8_t pct)
{
    const uint32_t maxDuty = (1u << HEATER_PWM_RES_BITS) - 1u;
    return (maxDuty * (uint32_t)pct) / 100u;
}

void pwm_stop()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (g_heaterPwmRunning)
    {
        ledcWrite((uint8_t)OVEN_HEATER, 0);
        ledcDetach((uint8_t)OVEN_HEATER);
        g_heaterPwmRunning = false;
        T15_INFO(t15_log::TEST_2_3, "Heater PWM detached (stopped)\n");
    }
#else
    static constexpr uint8_t HEATER_PWM_CHANNEL = 0;
    if (g_heaterPwmRunning)
    {
        ledcWrite(HEATER_PWM_CHANNEL, 0);
        ledcDetachPin(OVEN_HEATER);
        g_heaterPwmRunning = false;
        T15_INFO(t15_log::TEST_2_3, "Heater PWM detached (stopped)\n");
    }
#endif

    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);
}

bool pwm_start(uint8_t dutyPercent)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    pinMode(OVEN_HEATER, OUTPUT);
    digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);

    (void)ledcDetach((uint8_t)OVEN_HEATER);
    g_heaterPwmRunning = false;

    const bool ok = ledcAttach((uint8_t)OVEN_HEATER,
                               (uint32_t)HEATER_PWM_FREQ_HZ,
                               (uint8_t)HEATER_PWM_RES_BITS);

    if (!ok)
    {
        T15_ERROR(t15_log::TEST_2_3,
                  "Heater ledcAttach FAILED (GPIO=%d, Freq=%luHz, Res=%dbit)\n",
                  OVEN_HEATER,
                  (unsigned long)HEATER_PWM_FREQ_HZ,
                  (int)HEATER_PWM_RES_BITS);
        pinMode(OVEN_HEATER, OUTPUT);
        digitalWrite(OVEN_HEATER, HEATER_SAFE_LEVEL);
        g_heaterPwmRunning = false;
        return false;
    }

    g_heaterPwmRunning = true;

    const uint32_t duty = heaterDutyFromPercent(dutyPercent);
    ledcWrite((uint8_t)OVEN_HEATER, 0);
    delay(2);
    ledcWrite((uint8_t)OVEN_HEATER, duty);
    delay(2);
    ledcWrite((uint8_t)OVEN_HEATER, duty);

    T15_INFO(t15_log::TEST_2_3,
             "Heater PWM attached. GPIO=%d Freq=%luHz Duty=%u%%\n",
             OVEN_HEATER,
             (unsigned long)HEATER_PWM_FREQ_HZ,
             (unsigned)dutyPercent);
    return true;
#else
    static constexpr uint8_t HEATER_PWM_CHANNEL = 0;

    ledcSetup(HEATER_PWM_CHANNEL, HEATER_PWM_FREQ_HZ, HEATER_PWM_RES_BITS);
    ledcAttachPin(OVEN_HEATER, HEATER_PWM_CHANNEL);
    g_heaterPwmRunning = true;

    const uint32_t duty = heaterDutyFromPercent(dutyPercent);
    ledcWrite(HEATER_PWM_CHANNEL, 0);
    delay(2);
    ledcWrite(HEATER_PWM_CHANNEL, duty);
    delay(2);
    ledcWrite(HEATER_PWM_CHANNEL, duty);

    T15_INFO(t15_log::TEST_2_3,
             "Heater PWM attached. GPIO=%d CH=%u Freq=%luHz Duty=%u%%\n",
             OVEN_HEATER,
             (unsigned)HEATER_PWM_CHANNEL,
             (unsigned long)HEATER_PWM_FREQ_HZ,
             (unsigned)dutyPercent);
    return true;
#endif
}

bool is_running()
{
    return g_heaterPwmRunning;
}

} // namespace t15_heater
