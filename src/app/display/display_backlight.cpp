#include "display/display_backlight.h"

#include <Arduino.h>

#include "display/display_hsd040bpn1.h"

namespace {

static bool g_backlight_initialized = false;
static uint8_t g_backlight_percent = 100;

} // namespace

void display_backlight_init(void) {
    if (g_backlight_initialized) {
        return;
    }

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    g_backlight_percent = 100;
    g_backlight_initialized = true;
}

void display_backlight_set_percent(uint8_t percent) {
    if (!g_backlight_initialized) {
        display_backlight_init();
    }

    g_backlight_percent = percent;
    digitalWrite(GFX_BL, HIGH);
}

uint8_t display_backlight_get_percent(void) {
    return g_backlight_percent;
}
