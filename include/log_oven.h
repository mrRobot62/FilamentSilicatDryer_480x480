#pragma once
#include <Arduino.h>
#include "log_core.h"

/**
 * OVEN logging macros.
 *
 * Controlled by compile-time flags:
 *   -DOVENINFO   -> enable OVEN_INFO(...)
 *   -DOVENDBG    -> enable OVEN_DBG(...)
 *   -DOVENWARN   -> enable OVEN_WARN(...)
 *   -DOVENERR    -> enable OVEN_ERR(...)
 *
 * Each level is independent:
 *   - Enabling DEBUG does NOT automatically enable INFO.
 */

#ifdef OVENINFO
#define OVEN_INFO(...)                          \
    do                                          \
    {                                           \
        logPrintf("OVEN", "INFO", __VA_ARGS__); \
    } while (0)
#else
#define OVEN_INFO(...) \
    do                 \
    {                  \
    } while (0)
#endif

#ifdef OVENDBG
#define OVEN_DBG(...)                            \
    do                                           \
    {                                            \
        logPrintf("OVEN", "DEBUG", __VA_ARGS__); \
    } while (0)
#else
#define OVEN_DBG(...) \
    do                \
    {                 \
    } while (0)
#endif

#ifdef OVENWARN
#define OVEN_WARN(...)                          \
    do                                          \
    {                                           \
        logPrintf("OVEN", "WARN", __VA_ARGS__); \
    } while (0)
#else
#define OVEN_WARN(...) \
    do                 \
    {                  \
    } while (0)
#endif

#ifdef OVENERR
#define OVEN_ERR(...)                          \
    do                                         \
    {                                          \
        logPrintf("OVEN", "ERR", __VA_ARGS__); \
    } while (0)
#else
#define OVEN_ERR(...) \
    do                \
    {                 \
    } while (0)
#endif