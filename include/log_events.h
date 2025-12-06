#pragma once
#include "log_core.h"

/**
 * Event logging macros.
 *
 * Controlled by compile-time flags:
 *   -DEVENTINFO   -> enable EVENT_INFO(...)
 *   -DEVENTDBG    -> enable EVENT_DBG(...)
 *   -DEVENTWARN   -> enable EVENT_WARN(...)
 *   -DEVENTERR    -> enable EVENT_ERR(...)
 *
 * Each level is independent:
 *   - Enabling DEBUG does NOT automatically enable INFO.

 * Example
 * void handleTouchEvent(int x, int y)
 * {
 *    EVENT_INFO("Touch event at x=%d, y=%d\n", x, y);
 *    EVENT_DBG("Raw touch adc values: x=%d, y=%d\n", x, y);
 * }
 *
 */

#ifdef EVENTINFO
#define EVENT_INFO(...)                                                                                                \
  do {                                                                                                                 \
    logPrintf("EVENT", "INFO", __VA_ARGS__);                                                                           \
  } while (0)
#else
#define EVENT_INFO(...)                                                                                                \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef EVENTDBG
#define EVENT_DBG(...)                                                                                                 \
  do {                                                                                                                 \
    logPrintf("EVENT", "DEBUG", __VA_ARGS__);                                                                          \
  } while (0)
#else
#define EVENT_DBG(...)                                                                                                 \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef EVENTWARN
#define EVENT_WARN(...)                                                                                                \
  do {                                                                                                                 \
    logPrintf("EVENT", "WARN", __VA_ARGS__);                                                                           \
  } while (0)
#else
#define EVENT_WARN(...)                                                                                                \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef EVENTERR
#define EVENT_ERR(...)                                                                                                 \
  do {                                                                                                                 \
    logPrintf("EVENT", "ERR", __VA_ARGS__);                                                                            \
  } while (0)
#else
#define EVENT_ERR(...)                                                                                                 \
  do {                                                                                                                 \
  } while (0)
#endif