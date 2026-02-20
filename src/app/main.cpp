#include <Arduino.h>
#include <lvgl.h>

// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "log_core.h"
#include "ui.h"
#include "ui/screens/screen_dbg_hw.h"
#include "ui/screens/screen_main.h"
#include "ui_events.h"

#include "wifi_net.h"
#include "wifi_secrets.h"

static uint32_t last_beat = 0;
static uint32_t last_tick_ms = 0;
// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02 = relay2
constexpr int HOST_TX_PIN = 40; // IO40 = relay1

#include "esp_heap_caps.h"

static void udp_diag_print() {
    Serial.printf(
        "[UDP/DIAG] freeHeap=%u internal=%u largestInt=%u psram=%u wifi=%d rssi=%d\n",
        (unsigned)ESP.getFreeHeap(),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (int)WiFi.status(),
        (int)WiFi.RSSI());
}

#if defined(WIFI_LOGGING_HOST_UDP)
#include "fsd_udp.h" // your renamed udp header

#include "esp_heap_caps.h"
#include "udp_config.h"
#include "udp_config_store.h"

void udp_log_selftest() {
    Serial.println("[UDP] selftest: sending 3 packets...");
    udp_log::send_cstr("[UDP] selftest packet 1\n");
    delay(50);
    udp_diag_print();
    udp_log::send_cstr("[UDP] selftest packet 2\n");
    delay(50);
    udp_diag_print();
    udp_log::send_cstr("[UDP] selftest packet 3\n");
    Serial.println("[UDP] selftest: done");
    udp_diag_print();
}
#endif

void setup() {
    Serial.begin(115200);

    delay(5000); // damit du sicher die Setup-Logs siehst

#if defined(DWIFI_LOGGING_CLIENT_UDP)
    Serial.println("[UDP] WIFI_LOGGING_CLIENT_UDP is ENABLED");
#else
    Serial.println("[UDP] WIFI_LOGGING_CLIENT_UDP is DISABLED");
#endif

    // --- T12: Load UDP/WiFi config from Preferences at boot (works without opening screen_udp) ---
    //
    // Why: UDP logging must work immediately after boot, even if the user never opens screen_udp.
    // We therefore load SSID/PW/IP/PORT from Preferences (NVS) here and configure the UDP transport.
    //
    // Visibility: We emit a small, privacy-safe boot report:
    // - whether Preferences config was found (Fall A vs. Fall B)
    // - which SSID is used
    // - which UDP target IP/port is used
    // (password is NOT printed; only its length).
    UdpLogConfig udpCfg{};
    const bool hasUdpCfg = udp_cfg_load(udpCfg);

    if (hasUdpCfg) {
        INFO("UDP CFG: loaded from Preferences (NVS)\n");
        INFO("UDP CFG: ssid='%s' pw_len=%u\n", udpCfg.ssid, (unsigned)strlen(udpCfg.password));
        INFO("UDP CFG: target=%s:%u\n", udpCfg.targetIp, (unsigned)udpCfg.targetPort);

        // Apply runtime config for other modules (UI can read back via udp_config_current()).
        udp_config_apply(udpCfg);

        // Configure UDP destination (IP + PORT) for WiFi logging.
        udp_log::configure(udpCfg.targetIp, udpCfg.targetPort);
    } else {
        WARN("UDP CFG: no Preferences config found -> using build defaults (Fall A)\n");

        // Keep backward compatible defaults (build flags / hardcoded defaults in fsd_udp.h).
        udp_config_reset();
    }

    // Choose WiFi credentials: Preferences (if present) else compile-time secrets.
    const char *wifiSsid = hasUdpCfg ? udpCfg.ssid : WIFI_SSID;
    const char *wifiPass = hasUdpCfg ? udpCfg.password : WIFI_PASS;

    INFO("WIFI: connecting to ssid='%s' (source=%s)\n",
         wifiSsid, hasUdpCfg ? "prefs" : "build");
    wifi_net::begin_sta(wifiSsid, wifiPass);
    wifi_net::wait_connected(12000);

    // --- T12: Post-connect summary (visible in UDP viewer) ---
    //
    // Reason: Early boot logs before WiFi connect are only visible on Serial.
    // After WL_CONNECTED, the same info is emitted again so it is also visible via UDP logging.
    {
        const UdpLogConfig &cur = udp_config_current();
        if (cur.isValid()) {
            INFO("UDP CFG (post-connect): ssid='%s' pw_len=%u target=%s:%u\n",
                 cur.ssid, (unsigned)strlen(cur.password), cur.targetIp, (unsigned)cur.targetPort);
        } else {
            WARN("UDP CFG (post-connect): runtime config not valid -> using build defaults\n");
        }
    }

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");
    INFO("Version: 0.3 - T12.0 + UDP-Logging\n");
    INFO("2026-02-13\n\n");

#if defined(WIFI_LOGGING_HOST_UDP)
    extern void udp_log_selftest(); // forward
    udp_log_selftest();
    INFO("[UDP] INFO-macro test line\n");
#endif

    oven_comm_init(Serial2, 115200, HOST_RX_PIN, HOST_TX_PIN);

    oven_init();
    ui_init();
    INFO("[MAIN] ui_init() OK\n");

    last_tick_ms = millis();
}

void loop() {
    static uint32_t last_ui_update = 0;
    static uint32_t last_ui_log = 0;

    // LVGL tick
    const uint32_t now = millis();
    const uint32_t elapsed = now - last_tick_ms;
    last_tick_ms = now;
    lv_tick_inc(elapsed);

    // IMPORTANT: Poll UART / protocol frequently (non-blocking)
    oven_comm_poll();

    // Oven tick (1 Hz internal)
    oven_tick();

    // UI update (4 Hz)
    if (now - last_ui_update >= 250) {

        OvenRuntimeState st;
        oven_get_runtime_state(&st);

        // Debug log (1 Hz)
        if (now - last_ui_log >= 1000) {
            last_ui_log = now;
            // DBG("[UI] remaining=%lus duration=%umin mode=%u postRem=%u\n",
            //     (unsigned long)st.secondsRemaining,
            //     (unsigned int)st.durationMinutes,
            //     (unsigned)st.mode,
            //     (unsigned)st.post.secondsRemaining);
        }
        last_ui_update = now;

        switch (screen_manager_current()) {
        case SCREEN_MAIN:
            screen_main_update_runtime(&st);
            break;
        case SCREEN_CONFIG:
            // currently has screen_config it's own logic
            break;
        case SCREEN_DBG_HW:
            screen_dbg_hw_update_runtime(&st);
            break;
        case SCREEN_LOG:
            // not implemented yed
            break;
        default:
            screen_main_update_runtime(&st);
            break;
        }
    }

    // Rendering
    lv_timer_handler();
    delay(5);
}