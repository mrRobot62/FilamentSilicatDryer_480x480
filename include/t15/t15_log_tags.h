#pragma once

#include "log_core.h"

// T15 log-classification helper for empirical test runs.
//
// Goal:
// - make UDP log filtering easier in the external log viewer
// - classify messages by test block / procedure step
// - keep usage lightweight and explicit
//
// Example:
//   T15_INFO("2.2", "Heater OFF at chamber=%.1fC hotspot=%.1fC\n", chamberC, hotspotC);
//   T15_WARN("2.2", "Peak reached after %lus\n", seconds);
//
// Resulting prefix:
//   [T15-2.2] ...

namespace t15_log
{
    static constexpr const char* BASE = "BASE";
    static constexpr const char* TEST_2_1 = "2.1";
    static constexpr const char* TEST_2_2 = "2.2";
    static constexpr const char* TEST_2_3 = "2.3";
    static constexpr const char* TEST_3_1 = "3.1";
    static constexpr const char* TEST_3_2 = "3.2";
    static constexpr const char* TEST_4_1 = "4.1";
    static constexpr const char* TEST_4_2 = "4.2";
    static constexpr const char* TEST_5_0 = "5.0";
}

// Usage:
//   T15_INFO(t15_log::TEST_2_2, "...\n");
//   T15_WARN(t15_log::TEST_3_1, "...\n");
//   T15_ERROR(t15_log::BASE, "...\n");
//   T15_DBG(t15_log::TEST_4_1, "...\n");

#define T15_INFO(tag, fmt, ...)  INFO("[T15-%s] " fmt,  tag, ##__VA_ARGS__)
#define T15_WARN(tag, fmt, ...)  WARN("[T15-%s] " fmt,  tag, ##__VA_ARGS__)
#define T15_ERROR(tag, fmt, ...) ERROR("[T15-%s] " fmt, tag, ##__VA_ARGS__)
#define T15_DBG(tag, fmt, ...)   DBG("[T15-%s] " fmt,   tag, ##__VA_ARGS__)
