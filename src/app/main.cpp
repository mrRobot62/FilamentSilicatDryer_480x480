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

#if defined(WIFI_LOGGING_HOST_UDP)
#include "fsd_udp.h" // your renamed udp header

void udp_log_selftest() {
    Serial.println("[UDP] selftest: sending 3 packets...");
    udp_log::send_cstr("[UDP] selftest packet 1\n");
    delay(50);
    udp_log::send_cstr("[UDP] selftest packet 2\n");
    delay(50);
    udp_log::send_cstr("[UDP] selftest packet 3\n");
    Serial.println("[UDP] selftest: done");
}
#endif

void setup() {
    Serial.begin(115200);

    delay(5000); // damit du sicher die Setup-Logs siehst

#if defined(WIFI_LOGGING_HOST_UDP)
    Serial.println("[UDP] WIFI_LOGGING_HOST_UDP is ENABLED");
#else
    Serial.println("[UDP] WIFI_LOGGING_HOST_UDP is DISABLED");
#endif

    wifi_net::begin_sta(WIFI_SSID, WIFI_PASS);
    wifi_net::wait_connected(12000);

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");
    INFO("Version: 0.3 - T12.0");
    INFO("2026-02-13");

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
