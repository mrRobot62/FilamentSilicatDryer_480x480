#pragma once
#include <Arduino.h>
#include <stdarg.h>

/**
 * Core logging helpers.
 * All UI / EVENT / other module log macros should use these functions.
 *
 * NOTE:
 * - Make sure Serial.begin(...) is called early in your setup().
 * - Format strings follow printf-style formatting.
 */

inline void logPrintPrefix(const char *tag, const char *level) {
  Serial.print("[");
  Serial.print(tag);
  Serial.print("/");
  Serial.print(level);
  Serial.print("] ");
}

inline void logVPrintf(const char *tag, const char *level, const char *fmt, va_list args) {
  logPrintPrefix(tag, level);
  Serial.vprintf(fmt, args);
}

inline void logPrintf(const char *tag, const char *level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logVPrintf(tag, level, fmt, args);
  va_end(args);
}