#include <Arduino.h>

#include "log_core.h"
#include "udp/fsd_udp.h"

#include "T15/t15_log_tags.h"
#include "sensors/sensor_ntc.h"
#include "T15/heater_io.h"

// --------------------------------------------------------------------
// T15 Step 3C
// T15-2.3 Overshoot map using modularized sensor/heater files.
//
// Purpose:
// - keep test harness in this file
// - move reusable sensor access into sensor_ntc.*
// - move reusable heater actuation into heater_io.*
// --------------------------------------------------------------------

// Build-time tuning knobs
#ifndef T15_TEST_DUTY_PERCENT
#define T15_TEST_DUTY_PERCENT 100
#endif

#ifndef T15_TEST_RESET_TEMP_C
#define T15_TEST_RESET_TEMP_C 28
#endif

static constexpr uint32_t SAMPLE_INTERVAL_MS = 250;
static constexpr uint32_t LOG_INTERVAL_MS = 1000;

static constexpr int kOffPointsC[] = {35, 37, 39, 41};
static constexpr size_t kOffPointCount = sizeof(kOffPointsC) / sizeof(kOffPointsC[0]);

enum class Test23State : uint8_t
{
    WAIT_FOR_COOL = 0,
    HEATING_TO_OFFPOINT,
    RUNOUT_AFTER_OFF,
    PEAK_REACHED,
    COMPLETE
};

static uint32_t g_lastSampleMs = 0;
static uint32_t g_lastLogMs = 0;

static Test23State g_test23State = Test23State::WAIT_FOR_COOL;
static size_t g_currentOffIndex = 0;

static uint32_t g_heaterOffMs = 0;
static int16_t g_chamberAtOff_dC = -32768;
static int16_t g_peakChamber_dC = -32768;
static uint32_t g_peakAtMs = 0;
static uint8_t g_coolingConfirmCount = 0;

static void resetCurrentRun()
{
    g_heaterOffMs = 0;
    g_chamberAtOff_dC = -32768;
    g_peakChamber_dC = -32768;
    g_peakAtMs = 0;
    g_coolingConfirmCount = 0;
}

static void updateTest23(const uint32_t now)
{
    const bool door_open = t15_sensor::is_door_open();
    const auto& s = t15_sensor::get_sample();

    if (door_open)
    {
        t15_heater::pwm_stop();

        if (g_test23State != Test23State::WAIT_FOR_COOL)
        {
            g_test23State = Test23State::WAIT_FOR_COOL;
            resetCurrentRun();
            T15_WARN(t15_log::TEST_2_3, "Door OPEN -> test paused / heater forced OFF\n");
        }
        return;
    }

    if (isnan(s.tempChamberC))
        return;

    if (g_currentOffIndex >= kOffPointCount)
    {
        t15_heater::pwm_stop();
        g_test23State = Test23State::COMPLETE;
        return;
    }

    const int currentOffPointC = kOffPointsC[g_currentOffIndex];

    switch (g_test23State)
    {
        case Test23State::WAIT_FOR_COOL:
        {
            if (s.tempChamberC <= (float)T15_TEST_RESET_TEMP_C)
            {
                resetCurrentRun();
                g_test23State = Test23State::HEATING_TO_OFFPOINT;
                t15_heater::pwm_start(T15_TEST_DUTY_PERCENT);

                T15_INFO(t15_log::TEST_2_3,
                         "Run %u/%u START: OFF-point=%dC reset_temp=%dC duty=%u%%\n",
                         (unsigned)(g_currentOffIndex + 1),
                         (unsigned)kOffPointCount,
                         currentOffPointC,
                         (int)T15_TEST_RESET_TEMP_C,
                         (unsigned)T15_TEST_DUTY_PERCENT);
            }
            break;
        }

        case Test23State::HEATING_TO_OFFPOINT:
        {
            if (s.tempChamberC >= (float)currentOffPointC)
            {
                t15_heater::pwm_stop();
                g_heaterOffMs = now;
                g_chamberAtOff_dC = s.cha_dC;
                g_peakChamber_dC = s.cha_dC;
                g_peakAtMs = now;
                g_coolingConfirmCount = 0;
                g_test23State = Test23State::RUNOUT_AFTER_OFF;

                T15_INFO(t15_log::TEST_2_3,
                         "Run %u OFF at Tch=%.2fC target_off=%dC rawCh=%d mV=%ld Ohm=%ld\n",
                         (unsigned)(g_currentOffIndex + 1),
                         s.tempChamberC,
                         currentOffPointC,
                         (int)s.rawChamber,
                         (long)s.cha_mV,
                         (long)s.cha_ohm);
            }
            break;
        }

        case Test23State::RUNOUT_AFTER_OFF:
        {
            if (s.cha_dC > g_peakChamber_dC)
            {
                g_peakChamber_dC = s.cha_dC;
                g_peakAtMs = now;
                g_coolingConfirmCount = 0;
            }
            else if (s.cha_dC < g_peakChamber_dC)
            {
                if (g_coolingConfirmCount < 255)
                    g_coolingConfirmCount++;
            }

            if (g_coolingConfirmCount >= 8)
            {
                const float offC = (g_chamberAtOff_dC == -32768) ? NAN : (g_chamberAtOff_dC / 10.0f);
                const float peakC = (g_peakChamber_dC == -32768) ? NAN : (g_peakChamber_dC / 10.0f);
                const float overshootC = (isnan(offC) || isnan(peakC)) ? NAN : (peakC - offC);
                const uint32_t timeToPeakMs = (g_peakAtMs >= g_heaterOffMs) ? (g_peakAtMs - g_heaterOffMs) : 0;

                T15_INFO(t15_log::TEST_2_3,
                         "Run %u RESULT: off=%dC Tch_off=%.2fC Tch_peak=%.2fC overshoot=%.2fC time_to_peak=%lums (~%lus)\n",
                         (unsigned)(g_currentOffIndex + 1),
                         currentOffPointC,
                         offC,
                         peakC,
                         overshootC,
                         (unsigned long)timeToPeakMs,
                         (unsigned long)(timeToPeakMs / 1000UL));

                g_test23State = Test23State::PEAK_REACHED;
            }

            break;
        }

        case Test23State::PEAK_REACHED:
        {
            g_currentOffIndex++;
            resetCurrentRun();

            if (g_currentOffIndex < kOffPointCount)
            {
                g_test23State = Test23State::WAIT_FOR_COOL;
                T15_INFO(t15_log::TEST_2_3,
                         "Preparing next run. Waiting for chamber <= %dC\n",
                         (int)T15_TEST_RESET_TEMP_C);
            }
            else
            {
                g_test23State = Test23State::COMPLETE;
                T15_INFO(t15_log::TEST_2_3, "All T15-2.3 runs complete\n");
            }
            break;
        }

        case Test23State::COMPLETE:
            t15_heater::pwm_stop();
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!udp::begin("CLIENT"))
        Serial.println("[T15] UDP init FAILED");

    T15_INFO(t15_log::BASE,
             "Boot Step 3C: T15-2.3 overshoot map with modularized sensor/heater files\n");

    t15_sensor::init_door();
    t15_sensor::init_i2c_and_ads();
    t15_heater::pwm_stop();

    T15_INFO(t15_log::BASE,
             "Setup complete. Close door and let chamber cool <= %dC to start T15-2.3\n",
             (int)T15_TEST_RESET_TEMP_C);
}

void loop()
{
    const uint32_t now = millis();

    if (now - g_lastSampleMs >= SAMPLE_INTERVAL_MS)
    {
        g_lastSampleMs = now;
        t15_sensor::sample_chamber_temperature();
        updateTest23(now);
    }

    if (now - g_lastLogMs >= LOG_INTERVAL_MS)
    {
        g_lastLogMs = now;

        const bool door_open = t15_sensor::is_door_open();
        const auto& s = t15_sensor::get_sample();

        const char* stateStr = "WAIT_FOR_COOL";
        switch (g_test23State)
        {
            case Test23State::WAIT_FOR_COOL: stateStr = "WAIT_FOR_COOL"; break;
            case Test23State::HEATING_TO_OFFPOINT: stateStr = "HEATING"; break;
            case Test23State::RUNOUT_AFTER_OFF: stateStr = "RUNOUT"; break;
            case Test23State::PEAK_REACHED: stateStr = "PEAK_REACHED"; break;
            case Test23State::COMPLETE: stateStr = "COMPLETE"; break;
        }

        const int currentOffPointC = (g_currentOffIndex < kOffPointCount) ? kOffPointsC[g_currentOffIndex] : -1;

        T15_INFO(t15_log::TEST_2_3,
                 "Door=%s state=%s run=%u/%u off_point=%dC rawHot=%d hot_mV=%ld hot_valid=%d Thot=%s rawCh=%d cha_mV=%ld cha_Ohm=%ld Tch=%.2fC heater=%s\n",
                 door_open ? "OPEN" : "CLOSED",
                 stateStr,
                 (unsigned)((g_currentOffIndex < kOffPointCount) ? (g_currentOffIndex + 1) : kOffPointCount),
                 (unsigned)kOffPointCount,
                 currentOffPointC,
                 (int)s.rawHotspot,
                 (long)s.hot_mV,
                 s.hotValid ? 1 : 0,
                 s.hotValid ? "valid" : "nanC",
                 (int)s.rawChamber,
                 (long)s.cha_mV,
                 (long)s.cha_ohm,
                 s.tempChamberC,
                 t15_heater::is_running() ? "ON" : "OFF");
    }
}
