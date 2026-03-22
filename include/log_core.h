#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#include "udp/fsd_udp.h" // your UDP helper header

#ifdef INFO
#undef INFO
#endif
#ifdef WARN
#undef WARN
#endif
#ifdef ERR
#undef ERR
#endif

/**
 * Core logging helpers.
 * All UI / EVENT / other module log macros should use these functions.
 *
 * NOTE:
 * - Make sure Serial.begin(...) is called early in your setup().
 * - Format strings follow printf-style formatting.
 */

// One-line buffer to ensure "one UDP packet per log line".
// Keep moderate to avoid stack blowups.
#ifndef LOG_CORE_LINEBUF_SIZE
#define LOG_CORE_LINEBUF_SIZE 512
#endif

inline void logWriteUdpIfEnabled(const char *s, size_t n) {
#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
    if (s && n) {
        udp::send_bytes(reinterpret_cast<const uint8_t *>(s), n);
    }
#else
    (void)s;
    (void)n;
#endif
}

inline void logWriteSerial(const char *s, size_t n) {
    if (s && n) {
        if (s[0] != ';' && n > 1) {
            Serial.write(reinterpret_cast<const uint8_t *>(s), n);
        }
    }
}

inline void logPrintPrefixToBuf(char *out, size_t out_size, const char *tag, const char *level) {
    if (!out || out_size == 0) {
        return;
    }
    // Always NUL-terminate
    int n = snprintf(out, out_size, "[%s/%s] ", tag, level);
    if (n < 0) {
        out[0] = '\0';
    }
}

inline void logVPrintf(const char *tag, const char *level, const char *fmt, va_list args) {
    char line[LOG_CORE_LINEBUF_SIZE];

    // Build prefix
    char prefix[64];
    logPrintPrefixToBuf(prefix, sizeof(prefix), tag, level);

    // Format message into line buffer (after prefix)
    // First copy prefix
    size_t p_len = strnlen(prefix, sizeof(prefix));
    if (p_len >= sizeof(line)) {
        p_len = sizeof(line) - 1;
    }
    memcpy(line, prefix, p_len);

    // Format the message after the prefix
    va_list args_copy;
    va_copy(args_copy, args);

    int n = vsnprintf(line + p_len, sizeof(line) - p_len, fmt, args_copy);
    va_end(args_copy);

    if (n < 0) {
        return;
    }

    // Total bytes to write (prefix + truncated message, excluding terminating '\0')
    size_t msg_len = (size_t)n;
    size_t total = p_len + msg_len;
    if (total >= sizeof(line)) {
        total = sizeof(line) - 1;
    }

    // Serial output (exactly as before, just via one buffer)
    logWriteSerial(line, total);

    // UDP output (one packet per log call)
    logWriteUdpIfEnabled(line, total);
}

inline void logPrintf(const char *tag, const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVPrintf(tag, level, fmt, args);
    va_end(args);
}

inline void logRawVPrintf(const char *fmt, va_list args) {
    char line[LOG_CORE_LINEBUF_SIZE];

    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(line, sizeof(line), fmt, args_copy);
    va_end(args_copy);

    if (n < 0) {
        return;
    }

    size_t total = (size_t)n;
    if (total >= sizeof(line)) {
        total = sizeof(line) - 1;
    }

    logWriteSerial(line, total);
    logWriteUdpIfEnabled(line, total);
}

inline void logRawPrintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logRawVPrintf(fmt, args);
    va_end(args);
}

#define RAW(...)                   \
    do {                           \
        logRawPrintf(__VA_ARGS__); \
    } while (0)

#define INFO(...)                               \
    do {                                        \
        logPrintf("MAIN", "INFO", __VA_ARGS__); \
    } while (0)

#define DBG(...)                                 \
    do {                                         \
        logPrintf("MAIN", "DEBUG", __VA_ARGS__); \
    } while (0)

#define WARN(...)                               \
    do {                                        \
        logPrintf("MAIN", "WARN", __VA_ARGS__); \
    } while (0)

#define ERR(...)                               \
    do {                                       \
        logPrintf("MAIN", "ERR", __VA_ARGS__); \
    } while (0)

// Alias for convenience (some modules prefer ERROR over ERR).
#define ERROR(...)                             \
    do {                                       \
        logPrintf("MAIN", "ERR", __VA_ARGS__); \
    } while (0)

// END OF FILE