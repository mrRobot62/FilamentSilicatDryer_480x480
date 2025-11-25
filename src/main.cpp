#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// -----------------------------------------------------------------------------
// Pins: ESP32-4848S040 / dein Board
// (aus offiziellen Arduino_GFX Beispielen & Issues für dieses Panel)
// -----------------------------------------------------------------------------

// Backlight optional – bei dir vermutlich fest verdrahtet, aber Pin 38
#define GFX_BL 38

// SPI-Bus für ST7701-Register (CS/SCK/SDA)
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED, // DC – wird für ST7701 nicht benötigt
    39,              // CS
    48,              // SCK
    47,              // MOSI (SDA)
    GFX_NOT_DEFINED  // MISO – nicht benutzt
);

// RGB-Panel (DE/VS/HS/PCLK + 16-Bit RGB565 + Timings)
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
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

    // Timing / Polarität (typische Werte für HSD040BPN1-A00 / ST7701)
    1, 10, 8, 50, // hsync_polarity, front_porch, pulse_width, back_porch
    1, 10, 8, 20  // vsync_polarity, front_porch, pulse_width, back_porch
);

// Display-Objekt für ST7701 (480x480)
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480, // width, height
    rgbpanel,
    0,    // rotation
    true, // auto_flush
    bus,
    GFX_NOT_DEFINED, // RST – auf deinem Board RC-Reset-Schaltung
    st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations));

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== Arduino_GFX 1.5.0 minimal test (ST7701 480x480) ===");

    if (psramFound())
    {
        Serial.printf("PSRAM found, free: %u bytes\n", ESP.getFreePsram());
    }
    else
    {
        Serial.println("PSRAM NOT found!");
    }

    // Display initialisieren
    gfx->begin();
    gfx->fillScreen(BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH); // falls Backlight-Pin existiert
#endif

    // Test: Text ausgeben
    gfx->setCursor(40, 40);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->println("GFX 1.5.0 ST7701 test");

    Serial.println("Setup done, now cycling colors...");
}

// -----------------------------------------------------------------------------
// LOOP – einfache Farbtests
// -----------------------------------------------------------------------------

void loop()
{
    gfx->fillScreen(RED);
    delay(500);

    gfx->fillScreen(GREEN);
    delay(500);

    gfx->fillScreen(BLUE);
    delay(500);

    gfx->fillScreen(BLACK);
    delay(500);
}