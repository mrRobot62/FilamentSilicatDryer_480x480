#include "ui_events.h"
#include "ui.h"
#include "oven/oven.h"
#include "log_ui.h" // falls du Logging schon nutzt, sonst erstmal weglassen

void ui_event_main_start_button(lv_event_t *e)
{
    LV_UNUSED(e);

    // Minimal stub implementation for now:
    // Toggle oven running state and print something to Serial.

    if (oven_is_running())
    {
        oven_stop();
        Serial.println(F("[UI_EVENTS] Start button clicked: oven_stop()"));
    }
    else
    {
        oven_start();
        Serial.println(F("[UI_EVENTS] Start button clicked: oven_start()"));
    }

    // Später:
    // - OvenRuntimeState lesen
    // - screen_main_update_runtime(&state)
    // - log_ui_button_clicked("start")
}