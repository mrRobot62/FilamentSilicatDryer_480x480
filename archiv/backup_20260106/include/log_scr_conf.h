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

#ifdef SCRCONF
#define SCR_INFO(...)                          \
  do                                           \
  {                                            \
    logPrintf("SCRCONF", "INFO", __VA_ARGS__); \
  } while (0)
#else
#define SCR_CONF_INFO(...) \
  do                       \
  {                        \
  } while (0)
#endif

#ifdef SCRCONF
#define SCR_CONF_DBG(...)                       \
  do                                            \
  {                                             \
    logPrintf("SCRCONF", "DEBUG", __VA_ARGS__); \
  } while (0)
#else
#define SCR_CONF_DBG(...) \
  do                      \
  {                       \
  } while (0)
#endif

#ifdef SCRCONFWARN
#define SCR_CONF_WARN(...)                     \
  do                                           \
  {                                            \
    logPrintf("SCRCONF", "WARN", __VA_ARGS__); \
  } while (0)
#else
#define SCR_CONF_WARN(...) \
  do                       \
  {                        \
  } while (0)
#endif

#ifdef SCRCONFERR
#define SCR_CONF_ERR(...)                     \
  do                                          \
  {                                           \
    logPrintf("SCRCONF", "ERR", __VA_ARGS__); \
  } while (0)
#else
#define SCR_CONF_ERR(...) \
  do                      \
  {                       \
  } while (0)
#endif