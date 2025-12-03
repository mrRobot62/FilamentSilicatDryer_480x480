#pragma once
#include "log_core.h"

/**
 * UI logging macros.
 *
 * Controlled by compile-time flags:
 *   -DUIINFO   -> enable UI_INFO(...)
 *   -DUIDBG    -> enable UI_DBG(...)
 *   -DUIWARN   -> enable UI_WARN(...)
 *   -DUIERR    -> enable UI_ERR(...)
 *
 * Each level is independent:
 *   - Enabling DEBUG does NOT automatically enable INFO.
 */

#ifdef UIINFO
#define UI_INFO(...)                                                                                                   \
  do {                                                                                                                 \
    logPrintf("UI", "INFO", __VA_ARGS__);                                                                              \
  } while (0)
#else
#define UI_INFO(...)                                                                                                   \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef UIDBG
#define UI_DBG(...)                                                                                                    \
  do {                                                                                                                 \
    logPrintf("UI", "DEBUG", __VA_ARGS__);                                                                             \
  } while (0)
#else
#define UI_DBG(...)                                                                                                    \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef UIWARN
#define UI_WARN(...)                                                                                                   \
  do {                                                                                                                 \
    logPrintf("UI", "WARN", __VA_ARGS__);                                                                              \
  } while (0)
#else
#define UI_WARN(...)                                                                                                   \
  do {                                                                                                                 \
  } while (0)
#endif

#ifdef UIERR
#define UI_ERR(...)                                                                                                    \
  do {                                                                                                                 \
    logPrintf("UI", "ERR", __VA_ARGS__);                                                                               \
  } while (0)
#else
#define UI_ERR(...)                                                                                                    \
  do {                                                                                                                 \
  } while (0)
#endif