#pragma once
#include "log_core.h"
#include <Arduino.h>

/**
 * HOST logging macros.
 *
 * Controlled by compile-time flags:
 *   -DHOSTINFO   -> enable HOST_INFO(...)
 *   -DHOSTDBG    -> enable HOST_DBG(...)
 *   -DHOSTWARN   -> enable HOST_WARN(...)
 *   -DHOSTERR    -> enable HOST_ERR(...)
 *
 * Each level is independent:
 *   - Enabling DEBUG does NOT automatically enable INFO.
 */

#ifdef HOSTINFO
#define HOST_INFO(...)                          \
    do {                                        \
        logPrintf("HOST", "INFO", __VA_ARGS__); \
    } while (0)
#else
#define HOST_INFO(...) \
    do {               \
    } while (0)
#endif

#ifdef HOSTRAW
#define HOST_RAW(...)              \
    do {                           \
        logRawPrintf(__VA_ARGS__); \
    } while (0)
#else
#define HOST_RAW(...) \
    do {              \
    } while (0)
#endif

#ifdef HOSTDBG
#define HOST_DBG(...)                            \
    do {                                         \
        logPrintf("HOST", "DEBUG", __VA_ARGS__); \
    } while (0)
#else
#define HOST_DBG(...) \
    do {              \
    } while (0)
#endif

#ifdef HOSTWARN
#define HOST_WARN(...)                          \
    do {                                        \
        logPrintf("HOST", "WARN", __VA_ARGS__); \
    } while (0)
#else
#define HOST_WARN(...) \
    do {               \
    } while (0)
#endif

#ifdef HOSTERR
#define HOST_ERR(...)                          \
    do {                                       \
        logPrintf("HOST", "ERR", __VA_ARGS__); \
    } while (0)
#else
#define HOST_ERR(...) \
    do {              \
    } while (0)
#endif