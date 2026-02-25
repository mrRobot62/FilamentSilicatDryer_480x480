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
#include "versions.h"

// --- UDP logging (shared) ----------------------------------------------------
#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
#include "fsd_udp.h"
#endif

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

#if defined(DWIFI_LOGGING_ENABLE)
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

#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
    // Start WiFi + UDP logger (uses WIFI_SSID/WIFI_PASS from wifi_secrets.h internally)
    const bool ok = udp_log::begin("HOST");
    Serial.println("[UDP] WIFI_LOGGING_ENABLE is ENABLED");

    // simple selftest packets (helps to verify UDP path immediately)
    if (ok) {
        INFO("UDP established\n");
        // udp_log::send_cstr("[UDP] selftest packet 1 (HOST)\n");
        // delay(50);
        // INFO("Try UDP packet 2\n");
        // udp_log::send_cstr("[UDP] selftest packet 2 (HOST)\n");
        // delay(50);
        // INFO("Try UDP packet 3\n");
        // udp_log::send_cstr("[UDP] selftest packet 3 (HOST)\n");
    } else {
        ERROR("UDP can't establish WIFI_LOGGING_ENABLE !!!!");
    }
#else
    Serial.println("[UDP] WIFI_LOGGING_CLIENT_UDP is DISABLED");

#endif

    // Choose WiFi credentials: Preferences (if present) else compile-time secrets.
    // const char *wifiSsid = hasUdpCfg ? udpCfg.ssid : WIFI_SSID;
    // const char *wifiPass = hasUdpCfg ? udpCfg.password : WIFI_PASS;

    // INFO("WIFI: connecting to ssid='%s' (source=%s)\n",
    //      wifiSsid, hasUdpCfg ? "prefs" : "build");
    // wifi_net::begin_sta(wifiSsid, wifiPass);
    // wifi_net::wait_connected(12000);

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");
    INFO("%s\n", HOST_VERSION_NAME);
    INFO("%s\n\n", HOST_VERSION_DATE);

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