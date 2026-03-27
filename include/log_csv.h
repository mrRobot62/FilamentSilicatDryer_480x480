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
// Temperature output, millivolts, ohms, tempearture for hotSpot & Chamber, some state values
struct TEMP {
    static constexpr const char *PREFIX = "TEMP";
    // [CSV_<PREFIX>];rawHot;hotMilliVolts;tempHot_dC;rawChamber;chamberMilliVolts;tempChamber_dC;heater_on;door_open;state
    static constexpr const char *FMT =
        "[CSV_%s];%ld;%ld;%ld;%ld;%ld;%ld;%d;%d;%d\n";
};

// State output, door_open, heater, fan12V, fan230V, fan230V_slow, motor
struct CLIENT_STATE {
    static constexpr const char *PREFIX = "CLIENT_STATE";

    // ts;CSV_CLIENT_STATE;f12;f230;f230s;motor;heater;lamp;door;state
    static constexpr const char *FMT =
        "CSV_%s;%d;%d;%d;%d;%d;%d;%d;%u\n";
};

struct HOST_PLOT {
    static constexpr const char *PREFIX = "HOST_PLOT";

    // ts;CSV_HOST_PLOT;t_chamber_dC;t_hotspot_dC;t_target_dC;t_low_dC;t_high_dC;safety
    static constexpr const char *FMT =
        "CSV_%s;%ld;%ld;%ld;%ld;%ld;%d\n";
};

struct HOST_LOGIC {
    static constexpr const char *PREFIX = "HOST_LOGIC";

    // ts;CSV_HOST_LOGIC;mode;running;heater_req;heater_actual;door;safety;commAlive;linkSynced
    static constexpr const char *FMT =
        "CSV_%s;%u;%d;%d;%d;%d;%d;%d;%d\n";
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

#ifdef CSV_OUT
#define CSV_LOG_STATE(...)                                                         \
    do {                                                                           \
        CSV_LOG(csv::CLIENT_STATE::PREFIX, csv::CLIENT_STATE::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_STATE(...) \
    do {                  \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_HOST_PLOT(...)                                              \
    do {                                                                    \
        CSV_LOG(csv::HOST_PLOT::PREFIX, csv::HOST_PLOT::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_HOST_PLOT(...) \
    do {                      \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_HOST_LOGIC(...)                                                \
    do {                                                                       \
        CSV_LOG(csv::HOST_LOGIC::PREFIX, csv::HOST_LOGIC::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_HOST_LOGIC(...) \
    do {                       \
    } while (0)
#endif
