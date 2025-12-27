#pragma once

#include <cstdio> // for snprintf
#include <cstring>
#include <lvgl.h>

#include "log_ui.h"
#include "oven/oven.h" // Pfad ggf. anpassen zu deinem Projekt
#include "screen_base.h"

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
// int UI_TEMP_TARGET_TOLERANCE_C = 0; // +/- range around target
static int ui_temp_target_tolerance_c = 3;

static constexpr int UI_TEMP_TRI_W = 16;    // adjust later
static constexpr int UI_TEMP_TRI_H = 10;    // adjust later
static constexpr int UI_TEMP_TRI_GAP_Y = 4; // Abstand zur Bar
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

static constexpr uint32_t UI_PRESET_BOX_BG_HEX = 0x00A000; // etwas dunkler
static constexpr int UI_PRESET_BOX_BG_OPA = 200;           // dezenter
static constexpr int UI_PRESET_BOX_BORDER_OPA = 200;       // border weniger hart

static constexpr int UI_PRESET_BOX_CENTER_Y_OFFSET = 30; // 0=center, +above, -below

static constexpr int UI_PRESET_TEXT_GAP_Y = 2;      // spacing between lines
static constexpr int UI_PRESET_ID_TEXT_OPA = 170;   // dimmed white for "#id"
static constexpr int UI_PRESET_NAME_TEXT_OPA = 255; // full white

// --- Preset box text layout ---
static constexpr int UI_PRESET_TEXT_PAD_TOP = 3;
static constexpr int UI_PRESET_TEXT_PAD_BOTTOM = 2;
static constexpr int UI_PRESET_TEXT_LINE_GAP = 2;

// ID line (dimmer)
static constexpr int UI_PRESET_ID_OPA = 180;           // 0..255
static constexpr uint32_t UI_PRESET_ID_HEX = 0xE0E0E0; // slightly dimmed white

// --------------------------------------------------------
// Preset box (dial center) - geometry & typography
// --------------------------------------------------------
static constexpr int UI_PRESET_BOX_PAD_X = 10; // inner padding left/right
static constexpr int UI_PRESET_BOX_NAME_MAX_W = UI_PRESET_BOX_W - (2 * UI_PRESET_BOX_PAD_X);

// Font candidates (largest -> smallest). Fallbacks compile-safe.
static inline const lv_font_t *UI_PRESET_FONT_L = LV_FONT_DEFAULT;
static inline const lv_font_t *UI_PRESET_FONT_M = LV_FONT_DEFAULT;
static inline const lv_font_t *UI_PRESET_FONT_S = LV_FONT_DEFAULT;

// Font candidates (largest -> smallest). Compile-safe fallbacks.
static inline const lv_font_t *ui_preset_font_l() {
#if defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
    return &lv_font_montserrat_22;
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return LV_FONT_DEFAULT;
#endif
}

static inline const lv_font_t *ui_preset_font_m() {
#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
    return &lv_font_montserrat_18;
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return LV_FONT_DEFAULT;
#endif
}

static inline const lv_font_t *ui_preset_font_s() {
#if defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#elif defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return LV_FONT_DEFAULT;
#endif
}

// Page indices
enum UiPageIndex : uint8_t {
    UI_PAGE_MAIN = 0,
    UI_PAGE_CONFIG = 1,
    UI_PAGE_LOG = 2,
    // For now UI_PAGE_COUNT is defined as macro (3)
};

// Public API
// lv_obj_t *screen_main_create(lv_obj_t *parent);

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_main_create(lv_obj_t *parent);
lv_obj_t *screen_main_get_swipe_target(void); // NEW

#ifdef __cplusplus
}
#endif

void screen_main_update_runtime(const OvenRuntimeState *state);
void screen_main_refresh_from_runtime(void); // Called by your UI manager when the active screen changes
void screen_main_set_active_page(uint8_t page_index);

// Format seconds -> "HH:MM:SS"
static void format_hhmmss(uint32_t seconds, char *buf, size_t buf_size) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;

    if (h > 99) {
        h = 99; // just in case
    }

    if (h > 0) {
        std::snprintf(buf, buf_size, "%02u:%02u:%02u", h, m, s);
    } else {
        std::snprintf(buf, buf_size, "%02u:%02u", m, s);
    }
}

// Format seconds -> "HH:MM" (for compact display)
static void format_hhmm(uint32_t seconds, char *buf, size_t buf_size) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;

    if (h > 99) {
        h = 99;
    }

    if (h > 0) {
        std::snprintf(buf, buf_size, "%02u:%02u", h, m);
    } else {
        std::snprintf(buf, buf_size, "00:%02u", m);
    }
}
