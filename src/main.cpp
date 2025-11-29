#include <Arduino.h>
#include <lvgl.h>
// #include <esp_heap_caps.h>

// #include "display/display_hsd040bpn1.h"
// #include "touch.h"

#include "ui/ui.h"
#include "ui/ui_events.h"

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

    // Wichtig: LVGL mitteilen, wie viele Millisekunden vergangen sind
    lv_tick_inc(elapsed);

    lv_timer_handler();
    delay(5);

    if (now - last_beat > 5000)
    {
        last_beat = now;
        // Serial.println(F("[MAIN] loop() heartbeat"));
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