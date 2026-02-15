#pragma once
#include <Arduino.h>

#if defined(WIFI_LOGGING_HOST_UDP) || defined(WIFI_LOGGING_CLIENT_UDP)

#include <NetworkUdp.h> // provides NetworkUDP
#include <WiFi.h>

#ifndef WIFI_LOGGING_UDP_PORT
#define WIFI_LOGGING_UDP_PORT 10514
#endif

#ifndef WIFI_LOGGING_UDP_IP
// Must be provided via build flags: -DWIFI_LOGGING_UDP_IP=\"192.168.178.50\"
#define WIFI_LOGGING_UDP_IP "0.0.0.0"
#endif

namespace udp_log {

inline NetworkUDP &udp() {
    static NetworkUDP s_udp;
    return s_udp;
}

inline IPAddress &targetIp() {
    static IPAddress s_ip(0, 0, 0, 0);
    return s_ip;
}

inline bool &started() {
    static bool s_started = false;
    return s_started;
}

inline void ensure_started() {
    if (started()) {
        return;
    }

    IPAddress ip;
    if (!ip.fromString(WIFI_LOGGING_UDP_IP)) {
        started() = false;
        return;
    }
    targetIp() = ip;

    // Bind once. Use an ephemeral local port if supported; otherwise fixed.
    // Some cores don't like begin(0), so use a fixed safe port.
    udp().begin(45678);
    started() = true;
}

// inline void send_bytes(const uint8_t *data, size_t len) {
//     if (!data || len == 0) {
//         return;
//     }
//     if (WiFi.status() != WL_CONNECTED) {
//         return;
//     }

//     ensure_started();
//     if (!started()) {
//         return;
//     }

//     udp().beginPacket(targetIp(), (uint16_t)WIFI_LOGGING_UDP_PORT);
//     udp().write(data, len);
//     udp().endPacket();
// }

inline void send_bytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UDP] WiFi not connected");
        return;
    }

    ensure_started();
    if (!started()) {
        Serial.println("[UDP] Not started");
        return;
    }

    // Serial.println("[UDP] Sending packet"); // <<< DEBUG LINE

    udp().beginPacket(targetIp(), (uint16_t)WIFI_LOGGING_UDP_PORT);
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

namespace udp_log {
inline void send_bytes(const uint8_t *, size_t) {}
inline void send_cstr(const char *) {}
} // namespace udp_log

#endif