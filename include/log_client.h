#pragma once
#include <Arduino.h>
#include "log_core.h"

/**
 * CLIENT logging macros.
 *
 * Controlled by compile-time flags:
 *   -DCLIENTINFO   -> enable CLIENT_INFO(...)
 *   -DCLIENTDBG    -> enable CLIENT_DBG(...)
 *   -DCLIENTWARN   -> enable CLIENT_WARN(...)
 *   -DCLIENTERR    -> enable CLIENT_ERR(...)
 *
 * Each level is independent:
 *   - Enabling DEBUG does NOT automatically enable INFO.
 */

#ifdef CLIENTINFO
#define CLIENT_INFO(...)                          \
    do                                          \
    {                                           \
        logPrintf("CLIENT", "INFO", __VA_ARGS__); \
    } while (0)
#else
#define CLIENT_INFO(...) \
    do                 \
    {                  \
    } while (0)
#endif

#ifdef CLIENTDBG
#define CLIENT_DBG(...)                            \
    do                                           \
    {                                            \
        logPrintf("CLIENT", "DEBUG", __VA_ARGS__); \
    } while (0)
#else
#define CLIENT_DBG(...) \
    do                \
    {                 \
    } while (0)
#endif

#ifdef CLIENTWARN
#define CLIENT_WARN(...)                          \
    do                                          \
    {                                           \
        logPrintf("CLIENT", "WARN", __VA_ARGS__); \
    } while (0)
#else
#define CLIENT_WARN(...) \
    do                 \
    {                  \
    } while (0)
#endif

#ifdef CLIENTERR
#define CLIENT_ERR(...)                          \
    do                                         \
    {                                          \
        logPrintf("CLIENT", "ERR", __VA_ARGS__); \
    } while (0)
#else
#define CLIENT_ERR(...) \
    do                \
    {                 \
    } while (0)
#endif