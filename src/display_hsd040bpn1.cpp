#include "display_hsd040bpn1.h"

// Globale Display-Instanz
Arduino_RGB_Display *gfx = nullptr;

bool init_display()
{
    Serial.println(F("[DISPLAY] init_display() start"));

    // --- 1) SWSPI-Bus für ST7701-Register (SPI_WriteComm/Data-Äquivalent) ---
    // Pins: CS=39, SCK=48, MOSI=47  (MISO unbenutzt)
    static Arduino_DataBus *bus = new Arduino_SWSPI(
        GFX_NOT_DEFINED, // DC (für ST7701-Init nicht nötig)
        39,              // CS
        48,              // SCK
        47,              // MOSI (SDA)
        GFX_NOT_DEFINED  // MISO (nicht verwendet)
    );

    // --- 2) RGB-Panel (DE/VS/HS/PCLK + 16bit-RGB Bus) ---
    // Dies ist das Schema, das bei dir schon OK war.
    static Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
        // Timing-Signale
        18, // DE
        17, // VSYNC
        16, // HSYNC
        21, // PCLK

        // R0..R4
        11, 12, 13, 14, 0,

        // G0..G5
        8, 20, 3, 46, 9, 10,

        // B0..B4
        4, 5, 6, 7, 15,

        // HSYNC: polarity, front_porch, pulse_width, back_porch
        1, 10, 8, 50,

        // VSYNC: polarity, front_porch (VFP), pulse_width, back_porch (VBP)
        1, 12, 8, 15);

    // --- 3) High-Level Display-Objekt mit deiner Panel-Init-Tabelle ---
    gfx = new Arduino_RGB_Display(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        rgbpanel,
        0,               // rotation
        true,            // auto_flush
        bus,             // SWSPI-Bus für ST7701-Register
        GFX_NOT_DEFINED, // RST (Panel hat eigenes RC-Reset)
        st7701_hsd040bpn1_init_operations,
        sizeof(st7701_hsd040bpn1_init_operations));

    // --- 4) Display starten ---
    if (!gfx->begin(16'000'000)) // 16 MHz PCLK, lief bei dir stabil
    {
        Serial.println(F("[DISPLAY] gfx->begin() FAILED"));
        return false;
    }

    // Optional: Display einschalten (falls nötig)
    // gfx->displayOn();

    // --- 5) Backlight ---
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    Serial.println(F("[DISPLAY] init_display() OK"));
    return true;
}