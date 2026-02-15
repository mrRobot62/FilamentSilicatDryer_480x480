#pragma once
#include <Arduino.h>
#include <WiFi.h>

// Minimal WiFi connect helper.
// - Blocking connect with timeout
// - No reconnect logic (intentionally minimal)
// - Keeps the rest of the app unchanged

namespace wifi_net {

inline void begin_sta(const char *ssid, const char *pass) {
    if (!ssid || !ssid[0]) {
        Serial.println("[WIFI] SSID missing -> WiFi disabled");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // lower latency, more stable logging in practice

    Serial.print("[WIFI] Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);
}

inline bool wait_connected(uint32_t timeout_ms = 12000) {
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WIFI] Connected. IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("[WIFI] Connect timeout -> continuing without WiFi");
    return false;
}

inline bool is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

} // namespace wifi_net