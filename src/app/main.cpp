#include <Arduino.h>
#include <lvgl.h>

// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "log_core.h"
#include "ui.h"
#include "ui/screens/screen_main.h"
#include "ui_events.h"

static uint32_t last_beat = 0;
static uint32_t last_tick_ms = 0;
// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02 = relay2
constexpr int HOST_TX_PIN = 40; // IO40 = relay1

void setup() {
    Serial.begin(115200);
    delay(4000); // damit du sicher die Setup-Logs siehst

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");

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

        screen_main_update_runtime(&st);
    }

    // Rendering
    lv_timer_handler();
    delay(5);
}

// void loop() {

//     static uint32_t last_ui_update = 0;

//     // Wichtig: LVGL mitteilen, wie viele Millisekunden vergangen sind
//     uint32_t now = millis();
//     uint32_t elapsed = now - last_tick_ms;
//     last_tick_ms = now;
//     lv_tick_inc(elapsed);

//     // Oven-Tick (1Hz)
//     oven_tick();

//     // UI update (4Hz)
//     if (now - last_ui_update >= 250) {

//         OvenRuntimeState st;
//         oven_get_runtime_state(&st);

//         // Debug-Log, damit wir sehen, dass UI-Update läuft
//         if (now - last_ui_update >= 1000) {
//             INFO("UI update: remaining=%lus, duration=%umin",
//                  (unsigned long)st.secondsRemaining,
//                  (unsigned int)st.durationMinutes);
//         }
//         last_ui_update = now;
//         screen_main_update_runtime(&st);
//     }

//     // Rendering
//     lv_timer_handler(); // rendert den aktuellen Screen
//     delay(5);           // sehr wichtig für Taskwechsel → verhindert Watchdog

//---------------------------------------------------------
// Debug: Touch-Events über LVGL testen
//---------------------------------------------------------
// **Direkter Test des Hersteller-Touch-Treibers**
//   -> Wenn hier bei Berührung nichts kommt, liegt das Problem unterhalb von LVGL.

// if (touch_touched())
// {
//     // Serial.print(F("[RAW TOUCH] x="));
//     // Serial.print(touch_last_x);
//     // Serial.print(F(" y="));
//     // Serial.println(touch_last_y);

//     // optional: kleinen Punkt zeichnen, um visuell zu sehen, wo getoucht wurde
//     gfx->fillCircle(touch_last_x, touch_last_y, 3, YELLOW);
// }
// }