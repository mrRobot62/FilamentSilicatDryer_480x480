#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "panel_hsd040bpn1.h" // deine Init-Tabelle

// -----------------------------------------------------------------------------
// Pins & Panel-Konfiguration für dein 4" ST7701 480x480 Panel
// -----------------------------------------------------------------------------

#define GFX_BL 38 // Backlight (falls verbunden)

// SPI-Bus für ST7701-Register (SWSPI)
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED, // DC (nicht benötigt für ST7701-Init)
    39,              // CS
    48,              // SCK
    47,              // MOSI (SDA)
    GFX_NOT_DEFINED  // MISO (nicht verwendet)
);

// RGB-Panel (DE/VS/HS/PCLK + 16bit RGB-Bus)
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

    // Hsync: polarity, front_porch, pulse_width, back_porch
    1, 10, 8, 50,

    // Vsync: polarity, front_porch (VFP), pulse_width, back_porch (VBP)
    1, 12, 8, 15);

// High-Level-Display-Objekt mit unserer eigenen Init-Sequenz
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480,        // width, height
    rgbpanel,        // RGB panel
    0,               // rotation
    true,            // auto_flush
    bus,             // SPI-Bus für ST7701-Register
    GFX_NOT_DEFINED, // RST (Board nutzt RC-Reset)
    st7701_hsd040bpn1_init_operations,
    // st7701_type8_init_operations,
    //    sizeof(st7701_type8_init_operations)
    sizeof(st7701_hsd040bpn1_init_operations));

// -----------------------------------------------------------------------------
// Setup: statisches Testbild
// -----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("=== Arduino_GFX 1.5.0 + HSD040BPN1 ST7701 480x480 test ==="));

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH); // Backlight an
#endif

    if (!gfx->begin())
    {
        Serial.println(F("gfx->begin() FAILED!"));
        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(F("gfx->begin() OK"));

    // Hintergrund schwarz
    gfx->fillScreen(BLACK);

    // Oben: 3 Farbbalken (sollen klar ROT / GRUEN / BLAU sein)
    int barH = 40;
    int barW = 480 / 3;
    gfx->fillRect(0, 0, barW, barH, RED);
    gfx->fillRect(barW, 0, barW, barH, GREEN);
    gfx->fillRect(barW * 2, 0, barW, barH, BLUE);

    // Rahmen in voller Breite/Höhe
    gfx->drawRect(0, 0, 480, 480, WHITE);

    // Diagonalen
    gfx->drawLine(0, 0, 479, 479, WHITE);
    gfx->drawLine(0, 479, 479, 0, WHITE);

    // Kreise in der Mitte
    int cx = 240;
    int cy = 240;
    gfx->drawCircle(cx, cy, 40, YELLOW);
    gfx->drawCircle(cx, cy, 80, CYAN);
    gfx->fillCircle(cx, cy, 20, MAGENTA);

    // Text oben links
    gfx->setCursor(10, 60);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->println(F("Arduino_GFX 1.5.0 + custom init"));

    gfx->setTextSize(1);
    gfx->println(F("HSD040BPN1 ST7701 480x480"));

    // Text unten links
    gfx->setCursor(10, 460);
    gfx->setTextColor(WHITE);
    gfx->println(F("Full-screen test"));
}
void loop()
{
    // statisches Bild
    delay(1000);
}