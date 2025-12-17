#pragma once

#include <lvgl.h>
#include <cstdio> // for snprintf

#include "oven/oven.h" // Pfad ggf. anpassen zu deinem Projekt
#include "log_ui.h"

// Forward-declare your OvenRuntimeState.
// Adjust the include if you already have a header for this.
// struct OvenRuntimeState;

// Layout constants (geometry)
#define UI_SCREEN_WIDTH 480
#define UI_SCREEN_HEIGHT 480

#define UI_SIDE_PADDING 60
#define UI_TOP_PADDING 5
#define UI_BOTTOM_PADDING 5
#define UI_FRAME_PADDING 10

#define UI_DIAL_SIZE 300
#define UI_DIAL_MIN_TICKS 5

#define UI_TIME_BAR_WIDTH 360 // 480 - 2 * 60
#define UI_TIME_BAR_HEIGHT 14

#define UI_TEMP_SCALE_WIDTH 360
#define UI_TEMP_SCALE_HEIGHT 15
#define UI_TEMP_MIN_C 0
#define UI_TEMP_MAX_C 120

#define UI_START_BUTTON_SIZE 100

#define UI_PAGE_COUNT 3
#define UI_PAGE_INDICATOR_HEIGHT 40
#define UI_PAGE_DOT_SIZE 10
#define UI_PAGE_DOT_SPACING 8

// Color constants as hex values (actual palette can be tuned later)
#define UI_COLOR_DANGER_HEX 0xFF0000 // unified red: STOP, WAIT blocked, DOOR open
#define UI_COLOR_BG_HEX 0x101010
#define UI_COLOR_PANEL_BG_HEX 0x202020
#define UI_COLOR_PAGE_DOT_ACTIVE_HEX 0xFFFFFF
#define UI_COLOR_PAGE_DOT_INACTIVE_HEX 0x808080
#define UI_COLOR_TIME_BAR_HEX 0x00AAFF
#define UI_COLOR_TEMP_TARGET_HEX 0xFF3333
#define UI_COLOR_TEMP_CURRENT_HEX 0x33FFAA

#define UI_COLOR_DIAL 0xFFFFFF
#define UI_COLOR_DIAL_TICKS_H 0xB0B0B0
#define UI_COLOR_DIAL_TICKS_M 0x909090
// #ffbf00
#define UI_COLOR_DIAL_FRAME 0xffbf00
#define UI_COLOR_DIAL_NEEDLE_MM 0x00aaff
#define UI_COLOR_DIAL_NEEDLE_HH 0xFF8800
// #00aaffff

// Dial colors (explicit, tweakable)
#define UI_COLOR_DIAL_LABELS_HEX 0xf0f0f0      // numbers
#define UI_COLOR_DIAL_TICKS_MAJOR_HEX 0xE0E0E0 // major ticks
#define UI_COLOR_DIAL_TICKS_MINOR_HEX 0xA0A0A0 // minor ticks
#define UI_COLOR_DIAL_MARKER_HEX 0x00AAFF      // small triangle marker (12 o'clock)

#define UI_COLOR_ICON_OFF_HEX 0xFFFFFF       // white
#define UI_COLOR_ICON_ON_HEX 0x00FF00        // green
#define UI_COLOR_ICON_DOOR_OPEN_HEX 0xFF0000 // red
#define UI_COLOR_ICON_DIMMED_HEX 0x505050    // optional, falls du später "disabled" brauchst
// --------------------------------------------------------
// Pause button colors (UI uses ui_color_from_hex() later)
// --------------------------------------------------------
static constexpr uint32_t UI_COL_PAUSE_RUNNING_HEX = 0xFFA500;                 // orange (PAUSE)
static constexpr uint32_t UI_COL_PAUSE_WAIT_BLOCKED_HEX = UI_COLOR_DANGER_HEX; // red (WAIT, door open -> blocked)
static constexpr uint32_t UI_COL_PAUSE_WAIT_READY_HEX = 0x00C000;              // green (WAIT, door closed -> can resume)
static constexpr uint32_t UI_COL_PAUSE_DISABLED_HEX = 0x404040;                // grey (STOPPED)

// --------------------------------------------------------
// Temperature triangle markers (geometry)
// --------------------------------------------------------
// Temperature triangle markers (geometry + behavior)
//int UI_TEMP_TARGET_TOLERANCE_C = 0; // +/- range around target
static int ui_temp_target_tolerance_c = 3;

static constexpr int UI_TEMP_TRI_W = 16;    // adjust later
static constexpr int UI_TEMP_TRI_H = 10;    // adjust later
static constexpr int UI_TEMP_TRI_GAP_Y = 4; // Abstand zur Bar
static constexpr int UI_TEMP_LABEL_GAP_X = 8;

// Temperature status colors (0xRRGGBB, will pass through ui_color_from_hex())
constexpr uint32_t UI_COLOR_TEMP_COLD_HEX = 0x00AAFF; // light blue
constexpr uint32_t UI_COLOR_TEMP_OK_HEX = 0x00C000;   // green
constexpr uint32_t UI_COLOR_TEMP_HOT_HEX = 0xFFA500;  // orange

// Page indices
enum UiPageIndex : uint8_t
{
    UI_PAGE_MAIN = 0,
    UI_PAGE_CONFIG = 1,
    UI_PAGE_LOG = 2,
    // For now UI_PAGE_COUNT is defined as macro (3)
};

// Public API
// void screen_main_create(lv_obj_t *parent);
lv_obj_t *screen_main_create(void);

void screen_main_update_runtime(const OvenRuntimeState *state);

// Called by your UI manager when the active screen changes
void screen_main_set_active_page(uint8_t page_index);

// Helpers
// UI color helper: swaps R <-> B (because the panel path swaps channels)
static lv_color_t ui_color_from_hex(uint32_t rgb_hex)
{
    // rgb_hex is 0xRRGGBB
    uint32_t r = (rgb_hex >> 16) & 0xFF;
    uint32_t g = (rgb_hex >> 8) & 0xFF;
    uint32_t b = (rgb_hex >> 0) & 0xFF;

    uint32_t swapped = (b << 16) | (g << 8) | (r << 0); // 0xBBGGRR
    return lv_color_hex(swapped);
}

// Format seconds -> "HH:MM:SS"
static void format_hhmmss(uint32_t seconds, char *buf, size_t buf_size)
{
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;

    if (h > 99)
        h = 99; // just in case

    if (h > 0)
    {
        std::snprintf(buf, buf_size, "%02u:%02u:%02u", h, m, s);
    }
    else
    {
        std::snprintf(buf, buf_size, "%02u:%02u", m, s);
    }
}

// Format seconds -> "HH:MM" (for compact display)
static void format_hhmm(uint32_t seconds, char *buf, size_t buf_size)
{
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;

    if (h > 99)
        h = 99;

    if (h > 0)
    {
        std::snprintf(buf, buf_size, "%02u:%02u", h, m);
    }
    else
    {
        std::snprintf(buf, buf_size, "00:%02u", m);
    }
}
