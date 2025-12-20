#pragma once

#include <lvgl.h>
#include <cstring>
#include <cstdio> // for snprintf

#include "oven/oven.h" // Adjust include path to match your project layout
#include "log_ui.h"

// -----------------------------
// Screen geometry (layout)
// -----------------------------
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

// -----------------------------
// Color constants (0xRRGGBB)
// -----------------------------
#define UI_COLOR_DANGER_HEX 0xFF0000 // STOP, WAIT blocked, DOOR open
#define UI_COLOR_BG_HEX 0x101010
#define UI_COLOR_PANEL_BG_HEX 0x202020
#define UI_COLOR_PAGE_DOT_ACTIVE_HEX 0xFFFFFF
#define UI_COLOR_PAGE_DOT_INACTIVE_HEX 0x808080
#define UI_COLOR_TIME_BAR_HEX 0x00AAFF
#define UI_COLOR_TEMP_TARGET_HEX 0xFF3333
#define UI_COLOR_TEMP_CURRENT_HEX 0x33FFAA

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
static int ui_temp_target_tolerance_c = 3;

static constexpr int UI_TEMP_TRI_W = 16;
static constexpr int UI_TEMP_TRI_H = 10;
static constexpr int UI_TEMP_TRI_GAP_Y = 4;
static constexpr int UI_TEMP_LABEL_GAP_X = 8;

// Temperature status colors (0xRRGGBB, will pass through ui_color_from_hex())
constexpr uint32_t UI_COLOR_TEMP_COLD_HEX = 0x00AAFF; // light blue
constexpr uint32_t UI_COLOR_TEMP_OK_HEX = 0x00C000;   // green
constexpr uint32_t UI_COLOR_TEMP_HOT_HEX = 0xFFA500;  // orange

// --------------------------------------------------------
// Preset display (center box in dial)
// --------------------------------------------------------
static constexpr int UI_PRESET_BOX_W = 180;
static constexpr int UI_PRESET_BOX_H = 46;
static constexpr int UI_PRESET_BOX_RADIUS = 10;
static constexpr int UI_PRESET_BOX_BORDER_W = 1;

static constexpr uint32_t UI_PRESET_BOX_BG_HEX = 0x00A000;
static constexpr int UI_PRESET_BOX_BG_OPA = 200;
static constexpr int UI_PRESET_BOX_BORDER_OPA = 200;

static constexpr int UI_PRESET_BOX_CENTER_Y_OFFSET = 30;

static constexpr int UI_PRESET_TEXT_PAD_TOP = 3;
static constexpr int UI_PRESET_TEXT_PAD_BOTTOM = 2;

static constexpr int UI_PRESET_BOX_PAD_X = 10;
static constexpr int UI_PRESET_BOX_NAME_MAX_W = UI_PRESET_BOX_W - (2 * UI_PRESET_BOX_PAD_X);

static constexpr int UI_PRESET_ID_OPA = 180;
static constexpr uint32_t UI_PRESET_ID_HEX = 0xE0E0E0;

static constexpr int UI_PRESET_NAME_TEXT_OPA = 255;

// Font candidates (largest -> smallest). Compile-safe fallbacks.
static inline const lv_font_t *ui_preset_font_l()
{
#if defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
    return &lv_font_montserrat_22;
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return LV_FONT_DEFAULT;
#endif
}

static inline const lv_font_t *ui_preset_font_m()
{
#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
    return &lv_font_montserrat_18;
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return LV_FONT_DEFAULT;
#endif
}

static inline const lv_font_t *ui_preset_font_s()
{
#if defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#elif defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return LV_FONT_DEFAULT;
#endif
}

// Page indices
enum UiPageIndex : uint8_t
{
    UI_PAGE_MAIN = 0,
    UI_PAGE_CONFIG = 1,
    UI_PAGE_LOG = 2,
};

// -----------------------------
// Public API
// -----------------------------

// Create and return the main screen root object.
lv_obj_t *screen_main_create(void);

// Push latest oven runtime state into the UI (called periodically by UI manager).
void screen_main_update_runtime(const OvenRuntimeState *state);

// Update the page indicator (called when the active page changes).
void screen_main_set_active_page(uint8_t page_index);

// -----------------------------
// Helpers (header-local)
// -----------------------------

// Convert 0xRRGGBB to LVGL color, swapping R<->B for the current panel pipeline.
static lv_color_t ui_color_from_hex(uint32_t rgb_hex)
{
    uint32_t r = (rgb_hex >> 16) & 0xFF;
    uint32_t g = (rgb_hex >> 8) & 0xFF;
    uint32_t b = (rgb_hex >> 0) & 0xFF;

    uint32_t swapped = (b << 16) | (g << 8) | (r << 0);
    return lv_color_hex(swapped);
}

// Format seconds into "HH:MM:SS" (uses "MM:SS" if hours == 0).
static void format_hhmmss(uint32_t seconds, char *buf, size_t buf_size)
{
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;

    if (h > 99)
        h = 99;

    if (h > 0)
        std::snprintf(buf, buf_size, "%02u:%02u:%02u", h, m, s);
    else
        std::snprintf(buf, buf_size, "%02u:%02u", m, s);
}

// Format seconds into "HH:MM" (compact top bar display).
static void format_hhmm(uint32_t seconds, char *buf, size_t buf_size)
{
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;

    if (h > 99)
        h = 99;

    if (h > 0)
        std::snprintf(buf, buf_size, "%02u:%02u", h, m);
    else
        std::snprintf(buf, buf_size, "00:%02u", m);
}
