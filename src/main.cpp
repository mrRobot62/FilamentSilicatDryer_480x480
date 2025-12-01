#include <Arduino.h>
#include <lvgl.h>

// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "ui/ui.h"
#include "ui/ui_events.h"
#include "ui/screens/screen_main.h"

static uint32_t last_beat = 0;
static uint32_t last_tick_ms = 0;

void setup()
{
    Serial.begin(115200);
    delay(4000); // damit du sicher die Setup-Logs siehst

    Serial.println();
    Serial.println(F("=== ESP32-S3 + ST7701 480x480 + LVGL 9.4.x + Touch ==="));

    ui_init();
    Serial.println(F("[MAIN] ui_init() OK"));

    last_tick_ms = millis();
}

void loop()
{

    uint32_t now = millis();
    uint32_t elapsed = now - last_tick_ms;
    last_tick_ms = now;
    static uint32_t last_ui_update = 0;

    // 1) LVGL housekeeping
    lv_timer_handler(); // verarbeitet LVGL-Anliegen
    delay(5);           // sehr wichtig für Taskwechsel → verhindert Watchdog

    // 2) Wichtig: LVGL mitteilen, wie viele Millisekunden vergangen sind
    lv_tick_inc(elapsed);

    // 3) Oven tick (1 Hz)
    oven_tick();

    // 3) UI update (z. B. 4x pro Sekunde)
    if (now - last_ui_update >= 250)
    {
        last_ui_update = now;

        OvenRuntimeState st;
        oven_get_runtime_state(&st);

        screen_main_update_runtime(&st);
    }
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