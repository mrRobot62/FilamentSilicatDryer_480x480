#include "fsd_udp.h"

#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "wifi_secrets.h" // WIFI_SSID / WIFI_PASS

#ifndef WIFI_LOGGING_UDP_IP
#define WIFI_LOGGING_UDP_IP "192.168.0.112"
#endif

#ifndef WIFI_LOGGING_UDP_PORT
#define WIFI_LOGGING_UDP_PORT 10514
#endif

namespace udp_log {

static bool g_enabled = true;
static bool g_wifi_ok = false;
static bool g_udp_ok = false;

static WiFiUDP g_udp;
static IPAddress g_target_ip;
static uint16_t g_target_port = (uint16_t)WIFI_LOGGING_UDP_PORT;

static const char *g_role = "UNK";

static void apply_defaults_if_needed() {
    if (!g_target_ip) {
        g_target_ip.fromString(WIFI_LOGGING_UDP_IP);
    }
    if (g_target_port == 0) {
        g_target_port = (uint16_t)WIFI_LOGGING_UDP_PORT;
    }
}

bool is_enabled() {
    return g_enabled && g_wifi_ok && g_udp_ok;
}

void configure(const char *targetIp, uint16_t targetPort) {
    if (targetIp && targetIp[0] != '\0') {
        g_target_ip.fromString(targetIp);
    }
    if (targetPort != 0) {
        g_target_port = targetPort;
    }
}

static bool wifi_connect_blocking(const char *ssid, const char *pass, uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, pass);

    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - t0) > timeout_ms) {
            return false;
        }
        delay(100);
        yield();
    }
    return true;
}

bool begin(const char *roleLabel) {
    g_role = (roleLabel && roleLabel[0] != '\0') ? roleLabel : "UNK";

    Serial.print("[UDP] apply_defaults_if_needed() - role:");
    Serial.println(g_role);
    apply_defaults_if_needed();

    // If secrets are missing, disable cleanly
    if (!WIFI_SSID || WIFI_SSID[0] == '\0') {
        g_enabled = false;
        g_wifi_ok = false;
        g_udp_ok = false;
        Serial.println("[UDP] ERROR::WIFI SSID not configured");
        return false;
    }

    g_wifi_ok = wifi_connect_blocking(WIFI_SSID, WIFI_PASS, 8000);

    if (!g_wifi_ok) {
        g_enabled = false;
        g_udp_ok = false;
        Serial.println("[UDP] ERROR::g_wifi_ok = wifi_connect_blocking(WIFI_SSID, WIFI_PASS, 8000);");
        return false;
    }
    Serial.print("[UDP] WIFI connected to SSID: ");
    Serial.println(WIFI_SSID);

    // Bind a local port (can be same as target port)
    g_udp_ok = g_udp.begin(g_target_port);

    if (!g_udp_ok) {
        g_enabled = false;
        Serial.println("[UDP] ERROR::g_udp_ok = g_udp.begin(g_target_port);");
        return false;
    }
    Serial.println("[UDP] WIFI & UDP ready");

    return true;
}

void end() {
    if (g_udp_ok) {
        g_udp.stop();
    }
    g_udp_ok = false;
    g_wifi_ok = false;
    g_enabled = false;
    Serial.println("[UDP] end() - ok");
}

bool send_bytes(const uint8_t *data, size_t len) {
    if (!is_enabled() || !data || len == 0) {
        return false;
    }

    if (!g_target_ip) {
        apply_defaults_if_needed();
    }
    if (!g_target_ip || g_target_port == 0) {
        return false;
    }

    if (!g_udp.beginPacket(g_target_ip, g_target_port)) {
        return false;
    }
    const size_t written = g_udp.write(data, len);
    const int end_ok = g_udp.endPacket();
    return (written == len) && (end_ok == 1);
}

void diag_print() {
    // Safe diag print: never crashes if disabled
    Serial.printf("[UDP][%s] enabled=%d wifi=%d udp=%d wifi_status=%d ip=%s target=%s:%u\n",
                  g_role,
                  (int)g_enabled,
                  (int)g_wifi_ok,
                  (int)g_udp_ok,
                  (int)WiFi.status(),
                  WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "0.0.0.0",
                  g_target_ip ? g_target_ip.toString().c_str() : "0.0.0.0",
                  (unsigned)g_target_port);
}

} // namespace udp_log

#else // WIFI_LOGGING_ENABLE off

namespace udp_log {

bool is_enabled() { return false; }
void configure(const char *, uint16_t) {}
bool begin(const char *) { return false; }
void end() {}
bool send_bytes(const uint8_t *, size_t) { return false; }
void diag_print() {}

} // namespace udp_log

#endif