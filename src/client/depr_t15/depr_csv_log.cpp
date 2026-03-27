#include "csv_log.h"

#include <Arduino.h>

#include "log_core.h"

namespace t15_csv {
void emit_header_once() {
#ifdef LOG_CSV
    INFO("%lu;CSV_HEADER;test;state;run;run_count;off_point_c;door_open;heater_on;rawHot;hot_mV;hot_valid;ThotC;rawCh;cha_mV;cha_Ohm;TchC\n",
         (unsigned long)millis());
#endif
}

void emit_state(const char *testTag,
                uint32_t nowMs,
                const char *state,
                uint8_t runIndex1,
                uint8_t runCount,
                int offPointC,
                const t15_sensor::ChamberSample &sample,
                bool heaterOn,
                bool doorOpen) {
#ifdef LOG_CSV
    if (sample.hotValid) {
        INFO("%lu;CSV;%s;%s;%u;%u;%d;%u;%u;%d;%ld;%u;%.2f;%d;%ld;%ld;%.2f\n",
             (unsigned long)nowMs,
             testTag,
             state,
             (unsigned)runIndex1,
             (unsigned)runCount,
             offPointC,
             doorOpen ? 1u : 0u,
             heaterOn ? 1u : 0u,
             (int)sample.rawHotspot,
             (long)sample.hot_mV,
             sample.hotValid ? 1u : 0u,
             sample.tempHotspotC,
             (int)sample.rawChamber,
             (long)sample.cha_mV,
             (long)sample.cha_ohm,
             sample.tempChamberC);
    } else {
        INFO("%lu;CSV;%s;%s;%u;%u;%d;%u;%u;%d;%ld;%u;nan;%d;%ld;%ld;%.2f\n",
             (unsigned long)nowMs,
             testTag,
             state,
             (unsigned)runIndex1,
             (unsigned)runCount,
             offPointC,
             doorOpen ? 1u : 0u,
             heaterOn ? 1u : 0u,
             (int)sample.rawHotspot,
             (long)sample.hot_mV,
             sample.hotValid ? 1u : 0u,
             (int)sample.rawChamber,
             (long)sample.cha_mV,
             (long)sample.cha_ohm,
             sample.tempChamberC);
    }
#else
    (void)testTag;
    (void)nowMs;
    (void)state;
    (void)runIndex1;
    (void)runCount;
    (void)offPointC;
    (void)sample;
    (void)heaterOn;
    (void)doorOpen;
#endif
}

void emit_marker(const char *testTag,
                 uint32_t nowMs,
                 const char *marker,
                 uint8_t runIndex1,
                 int offPointC,
                 float tchC,
                 float thotC,
                 bool heaterOn,
                 bool doorOpen) {
#ifdef LOG_CSV
    if (isnan(thotC)) {
        INFO("%lu;CSV_MARK;%s;%s;%u;%d;%.2f;nan;%u;%u\n",
             (unsigned long)nowMs,
             testTag,
             marker,
             (unsigned)runIndex1,
             offPointC,
             tchC,
             heaterOn ? 1u : 0u,
             doorOpen ? 1u : 0u);
    } else {
        INFO("%lu;CSV_MARK;%s;%s;%u;%d;%.2f;%.2f;%u;%u\n",
             (unsigned long)nowMs,
             testTag,
             marker,
             (unsigned)runIndex1,
             offPointC,
             tchC,
             thotC,
             heaterOn ? 1u : 0u,
             doorOpen ? 1u : 0u);
    }
#else
    (void)testTag;
    (void)nowMs;
    (void)marker;
    (void)runIndex1;
    (void)offPointC;
    (void)tchC;
    (void)thotC;
    (void)heaterOn;
    (void)doorOpen;
#endif
}

} // namespace t15_csv
