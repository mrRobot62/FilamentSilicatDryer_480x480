#pragma once

#include <cstdio> // for snprintf
#include <cstring>
#include <lvgl.h>

#include "log_ui.h"
#include "oven.h" // Pfad ggf. anpassen zu deinem Projekt
#include "screen_base.h"
#include "screen_manager.h"

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

// END OF FILE
