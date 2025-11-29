#include <Arduino.h>
#include "ui/ui_events.h"
#include "ui/ui.h"

void ui_event_ButtonTest(lv_event_t *e)
{
    Serial.println(F("[UI EVENT] ButtonTest clicked"));
}