#pragma once

#include "log_core.h"

// ---------------------------------------------------------------
// CSV log is usable for plotting data in
// udp_log_viewer
//
// data is separated via semicolon
//
// format:
// <timestamp>;<CSV_PREFIX>;<data1>;<data2>;<data x>
// ---------------------------------------------------------------

namespace csv {

struct TEMP {
    static constexpr const char *PREFIX = "TEMP";
    // [CSV_<PREFIX>];rawHot;hotMilliVolts;tempHot_dC;rawChamber;chamberMilliVolts;tempChamber_dC;heater_on;door_open;state
    static constexpr const char *FMT =
        "[CSV_%s];%ld;%ld;%ld;%ld;%ld;%ld;%d;%d;%d\n";
};

} // namespace csv

#ifdef CSV_OUT
#define CSV_LOG(prefix, fmt, ...)        \
    do {                                 \
        RAW(fmt, prefix, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG(...) \
    do {             \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_TEMP(...)                                          \
    do {                                                           \
        CSV_LOG(csv::TEMP::PREFIX, csv::TEMP::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_TEMP(...) \
    do {                  \
    } while (0)
#endif
