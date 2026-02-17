#pragma once
#include <Arduino.h>

// UDP logging transport for Host/Client WiFi logging.
//
// T12 change: target IP and target port can now be configured at runtime
// (e.g. loaded from Preferences at boot and edited via screen_udp).
//
// Backward compatible:
// - If udp_log::configure(...) is never called, the build flags
//   WIFI_LOGGING_UDP_IP / WIFI_LOGGING_UDP_PORT (or their defaults) are used.

#if defined(WIFI_LOGGING_HOST_UDP) || defined(WIFI_LOGGING_CLIENT_UDP)

#include <NetworkUdp.h> // provides NetworkUDP
#include <WiFi.h>

#ifndef WIFI_LOGGING_UDP_PORT
#define WIFI_LOGGING_UDP_PORT 10514
#endif

#ifndef WIFI_LOGGING_UDP_IP
// Can be provided via build flags: -DWIFI_LOGGING_UDP_IP=\"192.168.178.50\"
#define WIFI_LOGGING_UDP_IP "0.0.0.0"
#endif

namespace udp_log {

// Singleton UDP instance.
inline NetworkUDP &udp() {
    static NetworkUDP s_udp;
    return s_udp;
}

// Runtime target IP string. If empty -> fall back to WIFI_LOGGING_UDP_IP.
inline const char *&targetIpStr() {
    static const char *s_ip_str = nullptr;
    return s_ip_str;
}

// Runtime target port. If 0 -> fall back to WIFI_LOGGING_UDP_PORT.
inline uint16_t &targetPort() {
    static uint16_t s_port = 0;
    return s_port;
}

// Parsed runtime IP address.
inline IPAddress &targetIp() {
    static IPAddress s_ip(0, 0, 0, 0);
    return s_ip;
}

// Whether UDP has been bound and target IP parsed.
inline bool &started() {
    static bool s_started = false;
    return s_started;
}

// Configure target IP/port at runtime.
// - ip_str must be a valid IPv4 string (e.g. "192.168.1.23")
// - port must be 1..65535
//
// Note: This does not start WiFi. It only configures the UDP destination.
inline void configure(const char *ip_str, uint16_t port) {
    targetIpStr() = ip_str;
    targetPort() = port;
    started() = false; // force re-parse/re-init on next send
}

// Resolve IP+port from runtime config or build defaults.
inline bool resolve_target(IPAddress &out_ip, uint16_t &out_port) {
    const char *ip_str = targetIpStr();
    if (!ip_str || ip_str[0] == 0) {
        ip_str = WIFI_LOGGING_UDP_IP;
    }

    uint16_t port = targetPort();
    if (port == 0) {
        port = (uint16_t)WIFI_LOGGING_UDP_PORT;
    }

    IPAddress ip;
    if (!ip.fromString(ip_str)) {
        return false;
    }

    out_ip = ip;
    out_port = port;
    return true;
}

// Bind UDP socket once and parse target IP.
// Safe to call repeatedly.
inline void ensure_started() {
    if (started()) {
        return;
    }

    IPAddress ip;
    uint16_t port = 0;
    if (!resolve_target(ip, port)) {
        started() = false;
        return;
    }

    targetIp() = ip;

    // Bind once. Use a fixed safe local port.
    // Some cores don't like begin(0), so avoid ephemeral port 0.
    udp().begin(45678);
    started() = true;
}

inline void send_bytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    ensure_started();
    if (!started()) {
        return;
    }

    // Destination port can be runtime-configured.
    uint16_t port = targetPort();
    if (port == 0) {
        port = (uint16_t)WIFI_LOGGING_UDP_PORT;
    }

    udp().beginPacket(targetIp(), port);
    udp().write(data, len);
    udp().endPacket();
}

inline void send_cstr(const char *s) {
    if (!s) {
        return;
    }
    send_bytes(reinterpret_cast<const uint8_t *>(s), strlen(s));
}

} // namespace udp_log

#else

// Disabled build: provide no-op stubs.
namespace udp_log {
inline void configure(const char *, uint16_t) {}
inline void send_bytes(const uint8_t *, size_t) {}
inline void send_cstr(const char *) {}
} // namespace udp_log

#endif
