#pragma once

#include <stdint.h>

#include "sensors/sensor_ntc.h"

namespace t15_csv
{
void emit_header_once();

void emit_state(const char* testTag,
                uint32_t nowMs,
                const char* state,
                uint8_t runIndex1,
                uint8_t runCount,
                int offPointC,
                const t15_sensor::ChamberSample& sample,
                bool heaterOn,
                bool doorOpen);

void emit_marker(const char* testTag,
                 uint32_t nowMs,
                 const char* marker,
                 uint8_t runIndex1,
                 int offPointC,
                 float tchC,
                 float thotC,
                 bool heaterOn,
                 bool doorOpen);
} // namespace t15_csv
