#include <Arduino.h>
#include <lvgl.h>

// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "log_core.h"
#include "host_parameters.h"
#include "ui.h"
#include "ui/screens/screen_dbg_hw.h"
#include "ui/screens/screen_boot.h"
#include "ui/screens/screen_main.h"
#include "ui/screens/screen_manager.h"
#include "ui_events.h"
#include "versions.h"

// --- UDP logging (shared) ----------------------------------------------------
#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
#include "udp/fsd_udp.h"
#endif

#include "wifi_net.h"
#include "wifi_secrets.h"

// The parameters screen increases LVGL redraw depth enough to exceed the
// Arduino default loopTask stack on ESP32-S3 during screen transitions.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static uint32_t last_beat = 0;
static uint32_t last_tick_ms = 0;
static uint32_t g_boot_ui_last_ms = 0;
static uint32_t g_boot_wifi_elapsed_ms = 0;
static uint8_t g_boot_progress_percent = 0;
// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02 = relay2
constexpr int HOST_TX_PIN = 40; // IO40 = relay1

constexpr uint32_t BOOT_SERIAL_WAIT_MS = 5000;
constexpr uint32_t BOOT_WIFI_WAIT_MS = 8000;
constexpr uint32_t BOOT_TOTAL_PROGRESS_MS = BOOT_SERIAL_WAIT_MS + BOOT_WIFI_WAIT_MS;

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
    oven_comm_init(Serial2, 115200, HOST_RX_PIN, HOST_TX_PIN);
    host_parameters_init();
    oven_init();
    ui_init();

    last_tick_ms = millis();
    g_boot_ui_last_ms = last_tick_ms;

    auto pump_boot_ui = []() {
        const uint32_t now = millis();
        const uint32_t elapsed = now - g_boot_ui_last_ms;
        g_boot_ui_last_ms = now;
        lv_tick_inc(elapsed);
        lv_timer_handler();
    };

    auto set_boot_progress_ms = [&](uint32_t elapsed_ms, const char *status) {
        if (elapsed_ms > BOOT_TOTAL_PROGRESS_MS) {
            elapsed_ms = BOOT_TOTAL_PROGRESS_MS;
        }
        const uint8_t percent = (uint8_t)((elapsed_ms * 100U) / BOOT_TOTAL_PROGRESS_MS);
        g_boot_progress_percent = percent;
        screen_boot_set_status(status);
        screen_boot_set_progress(percent);
        pump_boot_ui();
    };

    auto finish_boot_progress = [&](uint32_t duration_ms, const char *status) {
        const uint32_t start = millis();
        const uint8_t start_percent = g_boot_progress_percent;

        while ((millis() - start) < duration_ms) {
            const uint32_t elapsed = millis() - start;
            const uint32_t t_q1000 = (elapsed * 1000U) / duration_ms;
            const uint32_t inv_q1000 = 1000U - t_q1000;
            const uint32_t eased_q1000 = 1000U - ((inv_q1000 * inv_q1000) / 1000U);
            const uint8_t percent = start_percent + (uint8_t)(((100U - start_percent) * eased_q1000) / 1000U);
            g_boot_progress_percent = percent;
            screen_boot_set_status(status);
            screen_boot_set_progress(percent);
            pump_boot_ui();
            delay(20);
        }

        g_boot_progress_percent = 100;
        screen_boot_set_status(status);
        screen_boot_set_progress(100);
        pump_boot_ui();
    };

    auto boot_wait_with_progress = [&](uint32_t duration_ms, uint32_t base_ms, const char *status) {
        const uint32_t start = millis();
        while ((millis() - start) < duration_ms) {
            set_boot_progress_ms(base_ms + (millis() - start), status);
            delay(20);
        }
        set_boot_progress_ms(base_ms + duration_ms, status);
    };

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");
    INFO("%s\n", HOST_VERSION_NAME);
    INFO("%s\n\n", HOST_VERSION_DATE);

    screen_manager_show(SCREEN_BOOT);
    screen_boot_set_progress(0);
    screen_boot_set_status("Systemstart...");
    pump_boot_ui();

    boot_wait_with_progress(BOOT_SERIAL_WAIT_MS, 0, "Initialisierung...");

#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
    g_boot_wifi_elapsed_ms = 0;
    screen_boot_set_status("WiFi / UDP Initialisierung...");
    pump_boot_ui();

    // Start WiFi + UDP logger (uses WIFI_SSID/WIFI_PASS from wifi_secrets.h internally)
    const bool ok = udp::begin("HOST");
    Serial.println("[UDP] WIFI_LOGGING_ENABLE is ENABLED");

    if (ok) {
        INFO("UDP established\n");
    } else {
        ERROR("UDP can't establish WIFI_LOGGING_ENABLE !!!!");
    }

    if (g_boot_wifi_elapsed_ms < BOOT_WIFI_WAIT_MS) {
        finish_boot_progress(4000, ok ? "Netzwerk bereit..." : "Netzwerk Timeout...");
    }
#else
    Serial.println("[UDP] WIFI_LOGGING_CLIENT_UDP is DISABLED");
    set_boot_progress_ms(BOOT_TOTAL_PROGRESS_MS, "Netzwerk deaktiviert");
#endif

    screen_boot_set_status("System bereit");
    screen_boot_set_progress(100);
    pump_boot_ui();
    delay(150);
    pump_boot_ui();
    screen_manager_show(SCREEN_MAIN);
    pump_boot_ui();
}

extern "C" void app_boot_progress_wifi(uint32_t elapsed_ms, uint32_t timeout_ms) {
    LV_UNUSED(timeout_ms);

    if (screen_manager_current() != SCREEN_BOOT) {
        return;
    }

    if (elapsed_ms > BOOT_WIFI_WAIT_MS) {
        elapsed_ms = BOOT_WIFI_WAIT_MS;
    }

    g_boot_wifi_elapsed_ms = elapsed_ms;

    const uint32_t total_elapsed = BOOT_SERIAL_WAIT_MS + elapsed_ms;
    const uint8_t percent = (uint8_t)((total_elapsed * 100U) / BOOT_TOTAL_PROGRESS_MS);

    g_boot_progress_percent = percent;
    screen_boot_set_status("WiFi / UDP Initialisierung...");
    screen_boot_set_progress(percent);

    const uint32_t now = millis();
    const uint32_t delta = now - g_boot_ui_last_ms;
    g_boot_ui_last_ms = now;
    lv_tick_inc(delta);
    lv_timer_handler();
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
        case SCREEN_PARAMETERS:
            break;
        case SCREEN_BOOT:
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
