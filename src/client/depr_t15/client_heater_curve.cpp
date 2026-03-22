#include <Arduino.h>

#include "log_core.h"
#include "log_csv.h"
#include "udp/fsd_udp.h"

#include "csv_log.h"
#include "depr_T15/heater_io.h"
#include "depr_T15/t15_log_tags.h"
#include "sensors/sensor_ntc.h"

// --- Debug Instrumentation (T15 Step 2.4.2) ---
// Eindeutiger Event-Zähler, damit Zustandswechsel und wichtige Ereignisse
// in der Logausgabe eindeutig nachvollziehbar bleiben.
static uint32_t g_evt_counter = 0;
#define EVT() (++g_evt_counter)

// --------------------------------------------------------------------
// T15 Step 2.4
// T15-2.3 Overshoot map + structured CSV logging
// --------------------------------------------------------------------

#ifndef T15_TEST_DUTY_PERCENT
#define T15_TEST_DUTY_PERCENT 50
#endif

#ifndef T15_TEST_RESET_TEMP_C
#define T15_TEST_RESET_TEMP_C 28
#endif

static constexpr uint32_t SAMPLE_INTERVAL_MS = 250;
static constexpr uint32_t LOG_INTERVAL_MS = 1000;

static constexpr int kOffPointsC[] = {35, 37, 39, 41};
static constexpr size_t kOffPointCount = sizeof(kOffPointsC) / sizeof(kOffPointsC[0]);

enum class Test23State : uint8_t {
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

static const char *state_to_str() {
    switch (g_test23State) {
    case Test23State::WAIT_FOR_COOL:
        return "WAIT_FOR_COOL";
    case Test23State::HEATING_TO_OFFPOINT:
        return "HEATING";
    case Test23State::RUNOUT_AFTER_OFF:
        return "RUNOUT";
    case Test23State::PEAK_REACHED:
        return "PEAK_REACHED";
    case Test23State::COMPLETE:
        return "COMPLETE";
    }
    return "UNKNOWN";
}

static const int state_to_int() {
    switch (g_test23State) {
    case Test23State::WAIT_FOR_COOL:
    case Test23State::HEATING_TO_OFFPOINT:
    case Test23State::RUNOUT_AFTER_OFF:
    case Test23State::PEAK_REACHED:
    case Test23State::COMPLETE:
        return static_cast<uint8_t>(g_test23State);
        break;
    }
    return -1;
}

static void resetCurrentRun() {
    g_heaterOffMs = 0;
    g_chamberAtOff_dC = -32768;
    g_peakChamber_dC = -32768;
    g_peakAtMs = 0;
    g_coolingConfirmCount = 0;
}

static void csv_mark(const char *marker, uint32_t nowMs, float tchC, float thotC) {
    const uint8_t runIndex1 =
        (g_currentOffIndex < kOffPointCount) ? (uint8_t)(g_currentOffIndex + 1) : (uint8_t)kOffPointCount;
    const int offPointC = (g_currentOffIndex < kOffPointCount) ? kOffPointsC[g_currentOffIndex] : -1;

    t15_csv::emit_marker("T15-2.3",
                         nowMs,
                         marker,
                         runIndex1,
                         offPointC,
                         tchC,
                         thotC,
                         t15_heater::is_running(),
                         t15_sensor::is_door_open());
}

static void updateTest23(const uint32_t now) {
    const bool door_open = t15_sensor::is_door_open();
    const auto &s = t15_sensor::get_sample();

    if (door_open) {
        // -> Heizung AUS: Sicherheitsabschaltung oder OFF-Punkt erreicht
        t15_heater::pwm_stop();

        if (g_test23State != Test23State::WAIT_FOR_COOL) {
            // -> Zustandswechsel der State Machine
            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE %s -> WAIT_FOR_COOL\n",
                     (unsigned long)EVT(),
                     state_to_str());
            g_test23State = Test23State::WAIT_FOR_COOL;
            resetCurrentRun();
            T15_WARN(t15_log::TEST_2_3, "Door OPEN -> test paused / heater forced OFF\n");
            csv_mark("DOOR_OPEN_ABORT", now, s.tempChamberC, s.tempHotspotC);
        }
        return;
    }

    if (isnan(s.tempChamberC)) {
        return;
    }

    if (g_currentOffIndex >= kOffPointCount) {
        // -> Heizung AUS: Sicherheitsabschaltung oder OFF-Punkt erreicht
        t15_heater::pwm_stop();
        g_test23State = Test23State::COMPLETE;
        return;
    }

    const int currentOffPointC = kOffPointsC[g_currentOffIndex];

    // Zustandsmaschine für Test T15-2.3: steuert Heizzyklen, Abschaltpunkte und Nachlaufmessung
    switch (g_test23State) {
    // WAIT_FOR_COOL: Warten bis die Kammer ausreichend abgekühlt ist (Startbedingung für neuen Run)
    case Test23State::WAIT_FOR_COOL: {
        if (s.tempChamberC <= (float)T15_TEST_RESET_TEMP_C) {
            resetCurrentRun();

            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE WAIT_FOR_COOL -> HEATING_TO_OFFPOINT\n",
                     (unsigned long)EVT());
            // -> Zustandswechsel der State Machine
            g_test23State = Test23State::HEATING_TO_OFFPOINT;

            // -> Heizung EIN: Start eines neuen Heizzyklus
            t15_heater::pwm_start(T15_TEST_DUTY_PERCENT);

            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] Run %u/%u START: OFF-point=%dC reset_temp=%dC duty=%u%%\n",
                     (unsigned long)EVT(),
                     (unsigned)(g_currentOffIndex + 1),
                     (unsigned)kOffPointCount,
                     currentOffPointC,
                     (int)T15_TEST_RESET_TEMP_C,
                     (unsigned)T15_TEST_DUTY_PERCENT);
            csv_mark("RUN_START", now, s.tempChamberC, s.tempHotspotC);
        }
        break;
    }

    // HEATING_TO_OFFPOINT: Heizung läuft bereits – Ziel ist es den definierten Abschaltpunkt zu erreichen
    case Test23State::HEATING_TO_OFFPOINT: {
        if (s.tempChamberC >= (float)currentOffPointC) {
            // -> Heizung AUS: Sicherheitsabschaltung oder OFF-Punkt erreicht
            t15_heater::pwm_stop();
            g_heaterOffMs = now;
            g_chamberAtOff_dC = s.cha_dC;
            g_peakChamber_dC = s.cha_dC;
            g_peakAtMs = now;
            g_coolingConfirmCount = 0;

            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE HEATING_TO_OFFPOINT -> RUNOUT_AFTER_OFF\n",
                     (unsigned long)EVT());
            // -> Zustandswechsel der State Machine
            g_test23State = Test23State::RUNOUT_AFTER_OFF;

            T15_INFO(t15_log::TEST_2_3,
                     "Run %u OFF at Tch=%.2fC target_off=%dC rawCh=%d mV=%ld Ohm=%ld\n",
                     (unsigned)(g_currentOffIndex + 1),
                     s.tempChamberC,
                     currentOffPointC,
                     (int)s.rawChamber,
                     (long)s.cha_mV,
                     (long)s.cha_ohm);
            csv_mark("RUN_OFF", now, s.tempChamberC, s.tempHotspotC);
        }
        break;
    }

    // RUNOUT_AFTER_OFF: Heizung ist AUS – Nachlauf und Overshoot werden gemessen
    case Test23State::RUNOUT_AFTER_OFF: {
        if (s.cha_dC > g_peakChamber_dC) {
            g_peakChamber_dC = s.cha_dC;
            g_peakAtMs = now;
            g_coolingConfirmCount = 0;
        } else if (s.cha_dC < g_peakChamber_dC) {
            if (g_coolingConfirmCount < 255) {
                g_coolingConfirmCount++;
            }
        }

        if (g_coolingConfirmCount >= 8) {
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
            csv_mark("RUN_RESULT", now, peakC, s.tempHotspotC);

            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE RUNOUT_AFTER_OFF -> PEAK_REACHED\n",
                     (unsigned long)EVT());
            // -> Zustandswechsel der State Machine
            g_test23State = Test23State::PEAK_REACHED;
        }

        break;
    }

    // PEAK_REACHED: Peak erkannt – Auswertung abgeschlossen, Vorbereitung für nächsten Run
    case Test23State::PEAK_REACHED: {
        g_currentOffIndex++;
        resetCurrentRun();

        if (g_currentOffIndex < kOffPointCount) {
            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE PEAK_REACHED -> WAIT_FOR_COOL\n",
                     (unsigned long)EVT());
            // -> Zustandswechsel der State Machine
            g_test23State = Test23State::WAIT_FOR_COOL;
            T15_INFO(t15_log::TEST_2_3,
                     "Preparing next run. Waiting for chamber <= %dC\n",
                     (int)T15_TEST_RESET_TEMP_C);
            csv_mark("RUN_PREPARE_NEXT", now, s.tempChamberC, s.tempHotspotC);
        } else {
            T15_INFO(t15_log::TEST_2_3,
                     "[EVT#%lu] STATE PEAK_REACHED -> COMPLETE\n",
                     (unsigned long)EVT());
            // -> Zustandswechsel der State Machine
            g_test23State = Test23State::COMPLETE;
            T15_INFO(t15_log::TEST_2_3, "All T15-2.3 runs complete\n");
            csv_mark("RUNS_COMPLETE", now, s.tempChamberC, s.tempHotspotC);
        }
        break;
    }

    // COMPLETE: Alle Runs abgeschlossen – System bleibt im sicheren Zustand (Heater OFF)
    case Test23State::COMPLETE:
        // -> Heizung AUS: Sicherheitsabschaltung oder OFF-Punkt erreicht
        t15_heater::pwm_stop();
        break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!udp::begin("CLIENT")) {
        Serial.println("[T15] UDP init FAILED");
    }

    T15_INFO(t15_log::BASE,
             "Boot Step 2.4: T15-2.3 overshoot map with structured CSV logging\n");

    t15_sensor::init_door();
    t15_sensor::init_i2c_and_ads();
    // -> Heizung AUS: Sicherheitsabschaltung oder OFF-Punkt erreicht
    t15_heater::pwm_stop();

    t15_csv::emit_header_once();

    T15_INFO(t15_log::BASE,
             "Setup complete. Close door and let chamber cool <= %dC to start T15-2.3\n",
             (int)T15_TEST_RESET_TEMP_C);


    uint32_t start = millis();
    int counter = 10;
    uint32_t lastTick = start;
    while (millis() - start < 10000)
    {
        uint32_t now = millis();

        if (now - lastTick >= 1000)
        {
            lastTick += 1000;

            INFO("(%2d) waiting for start ... \n", counter);
            counter--;
        }
    }
    INFO("START Test....\n");
}

void loop() {
    const uint32_t now = millis();

    if (now - g_lastSampleMs >= SAMPLE_INTERVAL_MS) {
        g_lastSampleMs = now;
        t15_sensor::sample_chamber_temperature();
        updateTest23(now);
    }

    if (now - g_lastLogMs >= LOG_INTERVAL_MS) {
        g_lastLogMs = now;

        const bool door_open = t15_sensor::is_door_open();
        const auto &s = t15_sensor::get_sample();

        const uint8_t runIndex1 =
            (g_currentOffIndex < kOffPointCount) ? (uint8_t)(g_currentOffIndex + 1) : (uint8_t)kOffPointCount;
        const int currentOffPointC = (g_currentOffIndex < kOffPointCount) ? kOffPointsC[g_currentOffIndex] : -1;

        // rawHot;hotMilliVolts;tempHot_dC;rawChamber;chamberMilliVolts;tempChamber_dC;heater_on;door_open;state
        // "%ld;%ld;%ld;%ld;%ld;%ld;%d;%d;%d"
        CSV_LOG_TEMP(
            // HotNTC
            (long)s.rawHotspot,
            (long)s.hot_mV,
            (long)(s.tempHotspotC * 10),
            // ChamberNTC
            (long)s.rawChamber,
            (long)s.cha_mV,
            (long)(s.tempChamberC * 10),
            // statistic
            state_to_int(),
            t15_heater::is_running() ? 1 : 0,
            door_open ? 1 : 0);

        T15_INFO(t15_log::TEST_2_3,
                 "Door=%s state=%s run=%u/%u off_point=%dC rawHot=%d hot_mV=%ld hot_valid=%d Thot= %.2fC (%s) rawCh=%d cha_mV=%ld cha_Ohm=%ld Tch=%.2fC heater=%s\n",
                 door_open ? "OPEN" : "CLOSE",
                 state_to_str(),
                 (unsigned)runIndex1,
                 (unsigned)kOffPointCount,
                 currentOffPointC,
                 (int)s.rawHotspot,
                 (long)s.hot_mV,
                 s.hotValid ? 1 : 0,
                 s.tempHotspotC,
                 s.hotValid ? "valid" : "nanC",
                 (int)s.rawChamber,
                 (long)s.cha_mV,
                 (long)s.cha_ohm,
                 s.tempChamberC,
                 t15_heater::is_running() ? "ON" : "OFF");
        t15_csv::emit_state("T15-2.3",
                            now,
                            state_to_str(),
                            runIndex1,
                            (uint8_t)kOffPointCount,
                            currentOffPointC,
                            s,
                            t15_heater::is_running(),
                            door_open);
    }
}
