#pragma once
#include <Arduino.h>

// Color constants as hex values (actual palette can be tuned later)
#define UI_COLOR_DANGER_HEX 0xFF0000 // unified red: STOP, WAIT blocked, DOOR open
// #ffa200
#define UI_COLOR_WARNING_HEX 0xffa200
// #00b3ff
#define UI_COLOR_COOLDOWN_HEX 0x00b3ff
#define UI_COLOR_WHITE_FG_HEX 0xFFFFFF

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
// #008cff
#define UI_COLOR_DIAL_FRAME 0xffbf00
#define UI_COLOR_DIAL_FRAME_POST 0x008cff
#define UI_COLOR_DIAL_NEEDLE_MM 0x008cff
#define UI_COLOR_DIAL_NEEDLE_HH 0xFF8800
// #00aaffff
// #04ce12
#define UI_COLOR_LINK_SYNCED 0x04ce12

// Dial colors (explicit, tweakable)
#define UI_COLOR_DIAL_LABELS_HEX 0xf0f0f0      // numbers
#define UI_COLOR_DIAL_TICKS_MAJOR_HEX 0xE0E0E0 // major ticks
#define UI_COLOR_DIAL_TICKS_MINOR_HEX 0xA0A0A0 // minor ticks
#define UI_COLOR_DIAL_MARKER_HEX 0x00AAFF      // small triangle marker (12 o'clock)

#define UI_COLOR_ICON_OFF_HEX 0xFFFFFF         // white
#define UI_COLOR_ICON_ON_HEX 0x00FF00          // green
#define UI_COLOR_ICON_DOOR_OPEN_HEX 0xFF0000   // red
#define UI_COLOR_ICON_DOOR_CLOSED_HEX 0x00FF00 // red
#define UI_COLOR_ICON_DIMMED_HEX 0x505050      // optional, falls du später "disabled" brauchst

// Icon colors for CONFIG screen
static constexpr uint32_t ICON_OFF_HEX = 0xFFFFFF;      // white
static constexpr uint32_t ICON_ON_HEX = 0xFF8A00;       // orange (manual override ON)
static constexpr uint32_t ICON_DISABLED_HEX = 0x707070; // gray

// --------------------------------------------------------
// Pause button colors (UI uses ui_color_from_hex() later)
// --------------------------------------------------------
static constexpr uint32_t UI_COL_PAUSE_RUNNING_HEX = 0xFFA500;                 // orange (PAUSE)
static constexpr uint32_t UI_COL_PAUSE_WAIT_BLOCKED_HEX = UI_COLOR_DANGER_HEX; // red (WAIT, door open -> blocked)
static constexpr uint32_t UI_COL_PAUSE_WAIT_READY_HEX = 0x00C000;              // green (WAIT, door closed -> can resume)
static constexpr uint32_t UI_COL_PAUSE_DISABLED_HEX = 0x404040;                // grey (STOPPED)

// Temperature status colors (0xRRGGBB, will pass through ui_color_from_hex())
constexpr uint32_t UI_COLOR_TEMP_COLD_HEX = 0x00AAFF; // light blue
constexpr uint32_t UI_COLOR_TEMP_OK_HEX = 0x00C000;   // green
constexpr uint32_t UI_COLOR_TEMP_HOT_HEX = 0xFFA500;  // orange

constexpr int LV_OPA_15 = 15;
constexpr int LV_OPA_25 = 25;
constexpr int LV_OPA_35 = 35;

// END OF FILE