#include <Arduino.h>
#include <lvgl.h>

// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "log_core.h"
#include "ui/ui.h"
#include "ui/ui_events.h"
#include "ui/screens/screen_main.h"

static uint32_t last_beat = 0;
static uint32_t last_tick_ms = 0;

void setup()
{
    Serial.begin(115200);
    delay(4000); // damit du sicher die Setup-Logs siehst

    INFO("======================================================\n");
    INFO("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ===\n");
    INFO("======================================================\n");

    ui_init();
    INFO("[MAIN] ui_init() OK\n");

    last_tick_ms = millis();
}

void loop()
{

    static uint32_t last_ui_update = 0;

    // Wichtig: LVGL mitteilen, wie viele Millisekunden vergangen sind
    uint32_t now = millis();
    uint32_t elapsed = now - last_tick_ms;
    last_tick_ms = now;
    lv_tick_inc(elapsed);

    // Oven-Tick (1Hz)
    oven_tick();

    // UI update (4Hz)
    if (now - last_ui_update >= 250)
    {
        last_ui_update = now;

        OvenRuntimeState st;
        oven_get_runtime_state(&st);

        // Debug-Log, damit wir sehen, dass UI-Update läuft
        if (now - last_ui_update >= 1000)
        {
            INFO("UI update: remaining=%lus, duration=%umin",
                 (unsigned long)st.secondsRemaining,
                 (unsigned int)st.durationMinutes);
        }
        screen_main_update_runtime(&st);
    }

    // Rendering
    lv_timer_handler(); // rendert den aktuellen Screen
    delay(5);           // sehr wichtig für Taskwechsel → verhindert Watchdog

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
}