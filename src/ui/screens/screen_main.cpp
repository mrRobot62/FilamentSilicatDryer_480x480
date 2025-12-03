#include "screen_main.h"
#include <cstdio> // for snprintf

// -------------------------------------------------------------------
//
// Main-Screen
//
// Der mainScreen ist in vier Container aufgeteilt (von oben nach unten)
// Container 1:
// Anzeige ProgressBar der abgelaufen Zeit. Je länger der Balken desto
// kleiner remaining-Time
//
// Container 2:
//
//
// -------------------------------------------------------------------

// Internal widget storage
// diese Struktur enthält alle Widgets von allen Screens
typedef struct main_screen_widgets_t
{
    lv_obj_t *root;

    // --------------------------------------------------------
    // Containers
    // --------------------------------------------------------
    lv_obj_t *top_bar_container;
    lv_obj_t *center_container;
    lv_obj_t *page_indicator_container;
    lv_obj_t *bottom_container;

    // --------------------------------------------------------
    // Top bar
    // --------------------------------------------------------
    lv_obj_t *time_bar;
    lv_obj_t *time_label_remaining;

    // --------------------------------------------------------
    // Center
    // --------------------------------------------------------
    lv_obj_t *icons_container;
    lv_obj_t *dial_container;
    lv_obj_t *start_button_container;

    // --------------------------------------------------------
    // Icons
    // --------------------------------------------------------
    lv_obj_t *icon_fan12v;
    lv_obj_t *icon_fan230;
    lv_obj_t *icon_fan230_slow;
    lv_obj_t *icon_heater;
    lv_obj_t *icon_door;
    lv_obj_t *icon_motor;
    lv_obj_t *icon_lamp;

    // --------------------------------------------------------
    // Dial
    // --------------------------------------------------------
    lv_obj_t *dial; // TODO: connect to your existing dial implementation
    lv_obj_t *label_filament;
    lv_obj_t *label_time_in_dial;

    // --------------------------------------------------------
    // Start/Stop
    // --------------------------------------------------------
    lv_obj_t *btn_start;
    lv_obj_t *label_btn_start;

    // --------------------------------------------------------
    // Page indicator
    // --------------------------------------------------------
    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];

    // --------------------------------------------------------
    // Bottom: temperature
    // --------------------------------------------------------
    lv_obj_t *temp_scale;
    lv_obj_t *temp_label_current;
    lv_obj_t *temp_marker_target;
    lv_obj_t *temp_indicator_current;
} main_screen_widgets_t;

// Static instance
static main_screen_widgets_t ui;

// Forward declarations
static void create_top_bar(lv_obj_t *parent);
static void create_center_section(lv_obj_t *parent);
static void create_page_indicator(lv_obj_t *parent);
static void create_bottom_section(lv_obj_t *parent);

static void update_time_ui(const OvenRuntimeState &state);
static void update_dial_ui(const OvenRuntimeState &state);
static void update_temp_ui(const OvenRuntimeState &state);
static void update_actuator_icons(const OvenRuntimeState &state);

static void start_button_event_cb(lv_event_t *e);

//----------------------------------------------------
//
//----------------------------------------------------
static void test_fullscreen_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    UI_INFO("TEST_FULLSCREEN_BTN: code=%d", (int)code);
}

//----------------------------------------------------
//
//----------------------------------------------------
// Helpers
static lv_color_t color_from_hex(uint32_t hex)
{
    return lv_color_hex(hex);
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

//----------------------------------------------------
//
//----------------------------------------------------
// Public API: create the main screen
lv_obj_t *screen_main_create(void)
{
    // lv_obj_t *parent = lv_scr_act(); // use active screen as parent

    // Root object
    if (ui.root != nullptr)
    {
        UI_INFO("return screen_main_create()");
        return ui.root;
    }

    ui.root = lv_obj_create(nullptr);
    // ui.root = lv_screen_active();
    //  lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);

    lv_obj_set_style_bg_color(ui.root, color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    UI_DBG("[screen_main_create screen_main_create] screen-addr: %d\n", ui.root);

    // Create sub sections
    create_top_bar(ui.root);
    create_center_section(ui.root);
    create_page_indicator(ui.root);
    create_bottom_section(ui.root);

    UI_DBG("[screen_main_create screen_main_create] screen-addr: %d\n", ui.root);
    return ui.root;
}

//----------------------------------------------------
//
//----------------------------------------------------
// Public API: runtime update
//
void screen_main_update_runtime(const OvenRuntimeState *state)
{
    if (!state)
    {
        return;
    }

    update_time_ui(*state);
    update_dial_ui(*state);
    update_temp_ui(*state);
    update_actuator_icons(*state);
}

// Public API: page indicator update
void screen_main_set_active_page(uint8_t page_index)
{
    if (page_index >= UI_PAGE_COUNT)
    {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        lv_color_t col = (i == page_index)
                             ? color_from_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : color_from_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }
}

//----------------------------------------------------
// Section creation (Container)
//----------------------------------------------------

//----------------------------------------------------
//
//----------------------------------------------------
static void create_top_bar(lv_obj_t *parent)
{
    ui.top_bar_container = lv_obj_create(parent);
    lv_obj_set_size(ui.top_bar_container, UI_SCREEN_WIDTH, 50);
    lv_obj_align(ui.top_bar_container, LV_ALIGN_TOP_MID, 0, UI_TOP_PADDING);

    lv_obj_set_style_bg_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.top_bar_container, 0, 0);

    // Time progress bar
    ui.time_bar = lv_bar_create(ui.top_bar_container);
    lv_obj_set_size(ui.time_bar, UI_TIME_BAR_WIDTH, UI_TIME_BAR_HEIGHT);
    lv_obj_align(ui.time_bar, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING, 0);
    lv_bar_set_range(ui.time_bar, 0, 60 * 60); // default 60 min
    lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.time_bar, color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.time_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.time_bar, color_from_hex(UI_COLOR_TIME_BAR_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.time_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    // Remaining time label on the right
    ui.time_label_remaining = lv_label_create(ui.top_bar_container);
    lv_label_set_text(ui.time_label_remaining, "00:00");
    lv_obj_align(ui.time_label_remaining, LV_ALIGN_RIGHT_MID, -UI_SIDE_PADDING, 0);
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_center_section(lv_obj_t *parent)
{
    ui.center_container = lv_obj_create(parent);
    lv_obj_set_size(ui.center_container, UI_SCREEN_WIDTH, UI_DIAL_SIZE + 20);
    lv_obj_align(ui.center_container, LV_ALIGN_CENTER, 0, -10); // slight up offset

    lv_obj_set_style_bg_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.center_container, 0, 0);

    //
    // Icons container (left)
    //
    ui.icons_container = lv_obj_create(ui.center_container);
    lv_obj_set_size(ui.icons_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.icons_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.icons_container, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(ui.icons_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.icons_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // 12V fan
    ui.icon_fan12v = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_fan12v, "F12");

    // 230V fan (full speed)
    ui.icon_fan230 = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_fan230, "F230");

    // 230V fan slow
    ui.icon_fan230_slow = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_fan230_slow, "F230S");

    // Heater
    ui.icon_heater = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_heater, "H");

    // Door
    ui.icon_door = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_door, "D");

    // Motor
    ui.icon_motor = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_motor, "M");

    // Lamp
    ui.icon_lamp = lv_label_create(ui.icons_container);
    lv_label_set_text(ui.icon_lamp, "L");

    //
    // Dial container (center)
    //
    ui.dial_container = lv_obj_create(ui.center_container);
    lv_obj_set_size(ui.dial_container, UI_DIAL_SIZE, UI_DIAL_SIZE);
    lv_obj_align(ui.dial_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui.dial_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.dial_container, LV_OPA_TRANSP, 0);

    // TODO: here you should create or attach your existing 360° scale / dial implementation.
    // For now, we only create a simple placeholder object.
    ui.dial = lv_obj_create(ui.dial_container);
    lv_obj_set_size(ui.dial, UI_DIAL_SIZE, UI_DIAL_SIZE);
    lv_obj_center(ui.dial);
    lv_obj_set_style_bg_opa(ui.dial, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.dial, LV_OPA_TRANSP, 0);

    // Filament label in the center of the dial
    ui.label_filament = lv_label_create(ui.dial_container);
    lv_label_set_text(ui.label_filament, "#---");
    lv_obj_align(ui.label_filament, LV_ALIGN_CENTER, 0, -10);

    // Time label under filament label
    ui.label_time_in_dial = lv_label_create(ui.dial_container);
    lv_label_set_text(ui.label_time_in_dial, "00:00:00");
    lv_obj_align(ui.label_time_in_dial, LV_ALIGN_CENTER, 0, 12);

    //
    // Start/Stop button container (right)
    //
    ui.start_button_container = lv_obj_create(ui.center_container);
    lv_obj_set_size(ui.start_button_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.start_button_container, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.start_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.start_button_container, LV_OPA_TRANSP, 0);

    ui.btn_start = lv_btn_create(ui.start_button_container);
    lv_obj_set_size(ui.btn_start, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);
    lv_obj_center(ui.btn_start);
    lv_obj_add_event_cb(ui.btn_start, start_button_event_cb, LV_EVENT_CLICKED, nullptr);

    ui.label_btn_start = lv_label_create(ui.btn_start);
    lv_label_set_text(ui.label_btn_start, "START");
    lv_obj_center(ui.label_btn_start);
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_page_indicator(lv_obj_t *parent)
{
    ui.page_indicator_container = lv_obj_create(parent);
    lv_obj_set_size(ui.page_indicator_container, UI_SCREEN_WIDTH, UI_PAGE_INDICATOR_HEIGHT);
    lv_obj_align_to(ui.page_indicator_container, ui.center_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_set_style_bg_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_container, 0, 0);

    // Inner panel (rounded rectangle)
    ui.page_indicator_panel = lv_obj_create(ui.page_indicator_container);
    lv_obj_set_size(ui.page_indicator_panel, 100, 24); // width will auto-fit for 3 dots
    lv_obj_center(ui.page_indicator_panel);

    lv_obj_set_style_radius(ui.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui.page_indicator_panel, color_from_hex(UI_COLOR_PANEL_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_panel, 4, 0);

    lv_obj_set_flex_flow(ui.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        ui.page_dots[i] = lv_obj_create(ui.page_indicator_panel);
        lv_obj_set_size(ui.page_dots[i], UI_PAGE_DOT_SIZE, UI_PAGE_DOT_SIZE);

        lv_obj_set_style_bg_opa(ui.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ui.page_dots[i], UI_PAGE_DOT_SIZE / 2, 0);
        lv_obj_set_style_border_opa(ui.page_dots[i], LV_OPA_TRANSP, 0);

        lv_obj_set_style_margin_left(ui.page_dots[i], (i == 0) ? 0 : UI_PAGE_DOT_SPACING, 0);

        lv_color_t col = (i == UI_PAGE_MAIN)
                             ? color_from_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : color_from_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_bottom_section(lv_obj_t *parent)
{
    ui.bottom_container = lv_obj_create(parent);
    lv_obj_set_size(ui.bottom_container, UI_SCREEN_WIDTH, 70);
    lv_obj_align(ui.bottom_container, LV_ALIGN_BOTTOM_MID, 0, -UI_BOTTOM_PADDING);

    lv_obj_set_style_bg_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.bottom_container, 0, 0);

    // Temperature scale (horizontal bar as base)
    ui.temp_scale = lv_bar_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_scale, UI_TEMP_SCALE_WIDTH, UI_TEMP_SCALE_HEIGHT);
    lv_obj_align(ui.temp_scale, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING, 0);
    lv_bar_set_range(ui.temp_scale, UI_TEMP_MIN_C, UI_TEMP_MAX_C);
    lv_bar_set_value(ui.temp_scale, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(ui.temp_scale, color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.temp_scale, LV_OPA_COVER, LV_PART_MAIN);
    // Indicator is not used as "fill" here; we will use separate markers.

    // Target temperature marker (thin line)
    ui.temp_marker_target = lv_obj_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_marker_target, 2, UI_TEMP_SCALE_HEIGHT + 6);
    lv_obj_set_style_bg_color(ui.temp_marker_target, color_from_hex(UI_COLOR_TEMP_TARGET_HEX), 0);
    lv_obj_set_style_bg_opa(ui.temp_marker_target, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(ui.temp_marker_target, LV_OPA_TRANSP, 0);
    lv_obj_align_to(ui.temp_marker_target, ui.temp_scale, LV_ALIGN_OUT_TOP_LEFT, 0, -3);

    // Current temperature indicator (small rectangle above scale)
    ui.temp_indicator_current = lv_obj_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_indicator_current, 4, UI_TEMP_SCALE_HEIGHT + 6);
    lv_obj_set_style_bg_color(ui.temp_indicator_current, color_from_hex(UI_COLOR_TEMP_CURRENT_HEX), 0);
    lv_obj_set_style_bg_opa(ui.temp_indicator_current, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(ui.temp_indicator_current, LV_OPA_TRANSP, 0);
    lv_obj_align_to(ui.temp_indicator_current, ui.temp_scale, LV_ALIGN_OUT_TOP_LEFT, 0, -3);

    // Current temperature label on the right
    ui.temp_label_current = lv_label_create(ui.bottom_container);
    lv_label_set_text(ui.temp_label_current, "-- °C");
    lv_obj_align(ui.temp_label_current, LV_ALIGN_RIGHT_MID, -UI_SIDE_PADDING, 0);
}

//
// Runtime updates
//

//----------------------------------------------------
// update_time
// aktualisiert die Heiz-Zeit HH:MM:SS
// berechnet Restzeit
//----------------------------------------------------
static void update_time_ui(const OvenRuntimeState &state)
{
    const uint32_t totalSeconds = state.durationMinutes * 60;
    const uint32_t remaining = state.secondsRemaining;

    if (totalSeconds > 0)
    {
        uint32_t elapsed = (remaining <= totalSeconds) ? (totalSeconds - remaining) : totalSeconds;
        lv_bar_set_range(ui.time_bar, 0, totalSeconds);
        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    }
    else
    {
        lv_bar_set_range(ui.time_bar, 0, 1);
        lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);
    }

    char buf[16];
    format_hhmm(remaining, buf, sizeof(buf));
    lv_label_set_text(ui.time_label_remaining, buf);
}

//----------------------------------------------------
// update_dail
//
//----------------------------------------------------
static void update_dial_ui(const OvenRuntimeState &state)
{
    // Filament label (placeholder: "#<id>")
    char filament_buf[16];
    std::snprintf(filament_buf, sizeof(filament_buf), "#%u", static_cast<unsigned>(state.filamentId));
    lv_label_set_text(ui.label_filament, filament_buf);

    // Time label in dial (HH:MM:SS)
    char time_buf[16];
    format_hhmmss(state.secondsRemaining, time_buf, sizeof(time_buf));
    lv_label_set_text(ui.label_time_in_dial, time_buf);

    // TODO:
    // Here you need to update your existing 360° scale / needle based on remaining/total time.
    // This code just prepares the place where you hook into your custom dial drawing.
}

//----------------------------------------------------
// update_temp
// update der aktuellen Temperatur-Labels
// update der Scale-für IST-Temperatur
//----------------------------------------------------
static void update_temp_ui(const OvenRuntimeState &state)
{
    // TODO: set 'cur' and 'tgt' from your OvenRuntimeState fields
    int16_t cur = 0;
    int16_t tgt = 0;

    // Example (adjust to your real field names):
    // cur = (int16_t)state.temp_current;
    // tgt = (int16_t)state.temp_target;

    if (cur < UI_TEMP_MIN_C)
        cur = UI_TEMP_MIN_C;
    if (cur > UI_TEMP_MAX_C)
        cur = UI_TEMP_MAX_C;
    if (tgt < UI_TEMP_MIN_C)
        tgt = UI_TEMP_MIN_C;
    if (tgt > UI_TEMP_MAX_C)
        tgt = UI_TEMP_MAX_C;

    // Update label
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d °C", static_cast<int>(cur));
    lv_label_set_text(ui.temp_label_current, buf);

    // Compute positions along the scale width
    lv_coord_t scale_x = lv_obj_get_x(ui.temp_scale);
    lv_coord_t scale_y = lv_obj_get_y(ui.temp_scale);
    lv_coord_t scale_w = lv_obj_get_width(ui.temp_scale);

    auto value_to_x = [&](int16_t value) -> lv_coord_t
    {
        float t = (float)(value - UI_TEMP_MIN_C) / (float)(UI_TEMP_MAX_C - UI_TEMP_MIN_C);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;
        return scale_x + (lv_coord_t)(t * (float)scale_w);
    };

    lv_coord_t tgt_x = value_to_x(tgt);
    lv_coord_t cur_x = value_to_x(cur);

    lv_obj_set_pos(ui.temp_marker_target,
                   tgt_x - lv_obj_get_width(ui.temp_marker_target) / 2,
                   scale_y - 3);

    lv_obj_set_pos(ui.temp_indicator_current,
                   cur_x - lv_obj_get_width(ui.temp_indicator_current) / 2,
                   scale_y - 3);
}

//----------------------------------------------------
// update_actuator_icons
// führt update für Icons for (Farbwechsel)
// Icons signalisieren lediglich ON/OFF
//----------------------------------------------------
static void update_actuator_icons(const OvenRuntimeState &state)
{
    auto set_icon_color = [](lv_obj_t *obj, lv_color_t color)
    {
        lv_obj_set_style_text_color(obj, color, 0);
    };

    // Base colors
    const lv_color_t col_off = color_from_hex(UI_COLOR_ICON_OFF_HEX);
    const lv_color_t col_on = color_from_hex(UI_COLOR_ICON_ON_HEX);
    const lv_color_t col_door_open = color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX);

    // 12V fan: white (off) -> green (on)
    set_icon_color(ui.icon_fan12v, state.fan12v_on ? col_on : col_off);

    // 230V fan: white (off) -> green (on)
    set_icon_color(ui.icon_fan230, state.fan230_on ? col_on : col_off);

    // 230V fan slow: white (off) -> green (on)
    set_icon_color(ui.icon_fan230_slow, state.fan230_slow_on ? col_on : col_off);

    // Heater: white (off) -> green (on)
    set_icon_color(ui.icon_heater, state.heater_on ? col_on : col_off);

    // Door: white (closed) -> red (open)
    set_icon_color(ui.icon_door, state.door_open ? col_door_open : col_off);

    // Motor: white (off) -> green (on)
    set_icon_color(ui.icon_motor, state.motor_on ? col_on : col_off);

    // Lamp: white (off) -> green (on)
    set_icon_color(ui.icon_lamp, state.lamp_on ? col_on : col_off);
}

//
// Start/Stop button callback
//
//----------------------------------------------------
// start_button_event_cb
//
//----------------------------------------------------
static void start_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);

    // Wichtig: irgendein Log, das du sicher siehst
    UI_INFO("start_button_event_cb(): code=%d\n", (int)code);

    if (oven_is_running)
    {
        oven_stop();
        UI_INFO("OVEN_STOPPED\n");
    }
    else
    {
        oven_start();
        UI_INFO("OVEN_STARTED\n");
    }

    // TODO:
    // Here you should call into your oven control logic, e.g.:
    // if (oven_is_running()) oven_stop();
    // else oven_start();
    //
    // The visual state (START/STOP label, color) should then be updated
    // via screen_main_update_runtime() when OvenRuntimeState changes.
}