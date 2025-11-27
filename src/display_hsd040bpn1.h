#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "panel_hsd040bpn1.h" // enthält st7701_hsd040bpn1_init_operations

// Display-Auflösung
constexpr uint16_t SCREEN_WIDTH = 480;
constexpr uint16_t SCREEN_HEIGHT = 480;

// Backlight-Pin laut deinem Board
constexpr int GFX_BL = 38;

// Globales Display-Objekt, wird in der CPP definiert
extern Arduino_RGB_Display *gfx;

/**
 * Initialisiert:
 *  - ST7701 (über SWSPI)
 *  - ESP32-S3 RGB-Panel
 *  - Arduino_RGB_Display
 *  - Backlight
 *
 * @return true bei Erfolg, false bei Fehler
 */
bool init_display();