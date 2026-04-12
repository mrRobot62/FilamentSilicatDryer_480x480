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
struct CLIENT_TEMP {
    static constexpr const char *PREFIX = "CLIENT_PLOT";
    // [CSV_<PREFIX>];rawHot;hotMilliVolts;tempHot_dC;rawChamber;chamberMilliVolts;tempChamber_dC;heater_on;door_open;state
    static constexpr const char *FMT =
        "[CSV_%s];%ld;%ld;%ld;%ld;%ld;%ld;%d;%d;%d\n";
};

// State output, door_open, heater, fan12V, fan230V, fan230V_slow, motor
struct CLIENT_LOGIC {
    static constexpr const char *PREFIX = "CLIENT_LOGIC";

    // ts;[CSV_<PREFIX>];f12;f230;f230s;motor;heater;lamp;door;state
    static constexpr const char *FMT =
        "[CSV_%s];%d;%d;%d;%d;%d;%d;%d;%u\n";
};

struct HOST_TEMP {
    static constexpr const char *PREFIX = "HOST_PLOT";

    // ts;[CSV_<PREFIX>];t_chamber_dC;t_hotspot_dC;t_target_dC;t_low_dC;t_high_dC;safety
    static constexpr const char *FMT =
        "[CSV_%s];%ld;%ld;%ld;%ld;%ld;%d\n";
};

struct HOST_LOGIC {
    static constexpr const char *PREFIX = "HOST_LOGIC";

    // ts;[CSV_<PREFIX>];mode;running;heater_req;heater_actual;door;safety;commAlive;linkSynced;materialClass;heaterStage
    static constexpr const char *FMT =
        "[CSV_%s];%u;%d;%d;%d;%d;%d;%d;%d;%u;%u\n";
};

struct HOST_LONGRUN {
    static constexpr const char *PREFIX = "LR_HOST";

    // ts;[CSV_LR_HOST];window_s;samples;target_dC;tch_avg;tch_min;tch_max;thot_avg;thot_min;thot_max;
    // heater_req_on_count;heater_actual_on_count;
    // door_seen;safety_seen;comm_loss_seen;linksync_loss_seen
    static constexpr const char *FMT =
        "[CSV_%s];%u;%u;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%u;%u;%u;%u;%u;%u\n";
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
#define CSV_LOG_CLIENT_TEMP(...)                                                 \
    do {                                                                         \
        CSV_LOG(csv::CLIENT_TEMP::PREFIX, csv::CLIENT_TEMP::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_CLIENT_TEMP(...) \
    do {                         \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_CLIENT_LOGIC(...)                                                  \
    do {                                                                           \
        CSV_LOG(csv::CLIENT_LOGIC::PREFIX, csv::CLIENT_LOGIC::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_CLIENT_LOGIC(...) \
    do {                          \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_HOST_TEMP(...)                                               \
    do {                                                                     \
        CSV_LOG(csv::HOST_TEMP::PREFIX, csv::HOST_TEMP::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_HOST_TEMP(...) \
    do {                       \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_HOST_LOGIC(...)                                                \
    do {                                                                       \
        CSV_LOG(csv::HOST_LOGIC::PREFIX, csv::HOST_LOGIC::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_HOST_LOGIC(...) \
    do {                        \
    } while (0)
#endif

#ifdef CSV_OUT
#define CSV_LOG_HOST_LONGRUN(...)                                                \
    do {                                                                         \
        CSV_LOG(csv::HOST_LONGRUN::PREFIX, csv::HOST_LONGRUN::FMT, ##__VA_ARGS__); \
    } while (0)
#else
#define CSV_LOG_HOST_LONGRUN(...) \
    do {                          \
    } while (0)
#endif

// Compatibility aliases for older call sites.
#define CSV_LOG_TEMP(...) CSV_LOG_CLIENT_TEMP(__VA_ARGS__)
#define CSV_LOG_STATE(...) CSV_LOG_CLIENT_LOGIC(__VA_ARGS__)
#define CSV_LOG_HOST_PLOT(...) CSV_LOG_HOST_TEMP(__VA_ARGS__)
