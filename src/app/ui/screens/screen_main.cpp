#include "screen_main.h"
#include <cstdio> // for snprintf
#include "../icons/icons_32x32.h"
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
    lv_obj_t *dial;
    lv_obj_t *needleMM;
    lv_obj_t *needleHH;
    lv_obj_t *label_filament;
    lv_obj_t *label_time_in_dial;
    int32_t hours, minutes;

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
static void update_start_button_ui(void);

static void start_button_event_cb(lv_event_t *e);

static lv_style_t style_dial_border;

static lv_point_precise_t g_minute_hand_points[2];
static lv_point_precise_t g_hour_hand_points[2];

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

static int calc_minute_angle(int minute)
{
    return (90 - minute * 6);
}

static int calc_hour_angle(int hour, int minute)
{
    float hm = (float)hour + (float)(minute / 60.0f);
    return (90 - (int)(hm * 30));
}

static void update_needle(lv_obj_t *dial, lv_obj_t *needle, lv_point_precise_t *buf, int angle_deg, int rFrom, int rTo)
{
    lv_obj_t *parent = lv_obj_get_parent(needle);
    lv_area_t a;
    lv_obj_get_coords(dial, &a);

    lv_coord_t cx = (a.x1 + a.x2) / 2;
    lv_coord_t cy = (a.y1 + a.y2) / 2;

    float rad = angle_deg * 0.01745329252f; // PI/180

    // Startpunkt (innerer Radius)
    buf[0].x = cx + (int)(cosf(rad) * rFrom);
    buf[0].y = cy - (int)(sinf(rad) * rFrom);

    // Endpunkt (äußerer Radius)
    buf[1].x = cx + (int)(cosf(rad) * rTo);
    buf[1].y = cy - (int)(sinf(rad) * rTo);

    lv_line_set_points(needle, buf, 2);
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
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);

    lv_obj_set_style_bg_color(ui.root, color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    UI_DBG("[screen_main_create screen_main_create] screen-addr: %d\n", ui.root);

    lv_style_init(&style_dial_border);
    lv_style_set_border_width(&style_dial_border, 1);
    lv_style_set_border_color(&style_dial_border, lv_color_hex(0xFFFFFF)); // Weißer Rahmen
    lv_style_set_border_opa(&style_dial_border, LV_OPA_COVER);
    lv_style_set_radius(&style_dial_border, 12); // Optional: leicht abgerundet
    lv_style_set_pad_all(&style_dial_border, 0);

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
    update_start_button_ui();
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
    lv_obj_clear_flag(ui.top_bar_container, LV_OBJ_FLAG_SCROLLABLE);
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

    auto mk_scale_needle = [&](uint8_t width, lv_color_t color) -> lv_obj_t *
    {
        lv_obj_t *line = lv_line_create(parent);
        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_color(line, color, 0);
        return line;
    };

    auto mk_scale_needle_mutable = [&](lv_point_precise_t *pts, uint8_t width, lv_color_t color) -> lv_obj_t *
    {
        lv_obj_t *line = lv_line_create(parent);

        // WICHTIG: mutable Points setzen, damit lv_scale die Punkte in dieses Array schreibt
        lv_line_set_points_mutable(line, pts, 2);

        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_color(line, color, 0);
        return line;
    };

    ui.center_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.center_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.center_container, UI_SCREEN_WIDTH, UI_DIAL_SIZE + 20);
    lv_obj_align(ui.center_container, LV_ALIGN_CENTER, 0, -10); // slight up offset

    lv_obj_set_style_bg_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.center_container, 0, 0);

    //
    // Icons container (left)
    //
    ui.icons_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.icons_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.icons_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.icons_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.icons_container, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(ui.icons_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.icons_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // 32x32 white icons; we recolor them at runtime
    ui.icon_fan12v = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan12v, &fan12v_wht);
    lv_obj_set_size(ui.icon_fan12v, 32, 32);
    // fan12v is display-only: no click handler

    ui.icon_fan230 = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan230, &fan230v_fast_wht);
    lv_obj_set_size(ui.icon_fan230, 32, 32);

    ui.icon_fan230_slow = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan230_slow, &fan230v_low_wht);
    lv_obj_set_size(ui.icon_fan230_slow, 32, 32);

    ui.icon_heater = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_heater, &heater_wht);
    lv_obj_set_size(ui.icon_heater, 32, 32);

    ui.icon_door = lv_image_create(ui.icons_container);
    // For door we currently reuse the motor icon as placeholder; adjust if a dedicated door icon is added later
    lv_image_set_src(ui.icon_door, &door_open_wht);
    lv_obj_set_size(ui.icon_door, 32, 32);

    ui.icon_motor = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_motor, &motor230v);
    lv_obj_set_size(ui.icon_motor, 32, 32);

    ui.icon_lamp = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_lamp, &lamp230v_wht);
    lv_obj_set_size(ui.icon_lamp, 32, 32);
    // Lamp is user-interactive (touch toggles state), so keep its event callback wiring
    lv_obj_add_flag(ui.icon_lamp, LV_OBJ_FLAG_CLICKABLE);

    //
    // Dial container (center)
    //
    ui.dial_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.dial_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui.dial_container, &style_dial_border, 0);
    lv_obj_set_size(ui.dial_container, UI_DIAL_SIZE - 1, UI_DIAL_SIZE - 1);
    lv_obj_align(ui.dial_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui.dial_container, LV_OPA_TRANSP, 0);

    // UHR
    ui.dial = lv_scale_create(ui.dial_container);
    lv_obj_clear_flag(ui.dial, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.dial, UI_DIAL_SIZE, UI_DIAL_SIZE);
    lv_obj_set_style_radius(ui.dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(ui.dial, true, 0);
    lv_obj_align(ui.dial, LV_ALIGN_CENTER, 0, 0);

    // lv_obj_center(ui.dial);
    lv_scale_set_mode(ui.dial, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(ui.dial, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(ui.dial, /*lv_color_black()*/ lv_color_hex(UI_COLOR_DIAL), 0);
    lv_scale_set_label_show(ui.dial, true);
    lv_scale_set_total_tick_count(ui.dial, 61);
    lv_scale_set_major_tick_every(ui.dial, UI_DIAL_MIN_TICKS);
    static const char *hour_ticks[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", NULL};
    lv_scale_set_text_src(ui.dial, hour_ticks);
    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);

    // /* Label style properties */
    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    lv_style_set_text_color(&indicator_style, lv_palette_main(LV_PALETTE_YELLOW));

    // /* Major tick properties */
    lv_style_set_line_color(&indicator_style, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_length(&indicator_style, 12);    /* tick length */
    lv_style_set_line_width(&indicator_style, 3); /* tick width */
    lv_obj_add_style(ui.dial, &indicator_style, LV_PART_INDICATOR);

    // /* Minor tick properties */
    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    lv_style_set_line_color(&minor_ticks_style, lv_palette_main(LV_PALETTE_YELLOW));

    lv_style_set_length(&minor_ticks_style, 8);     /* tick length */
    lv_style_set_line_width(&minor_ticks_style, 2); /* tick width */
    lv_obj_add_style(ui.dial, &minor_ticks_style, LV_PART_ITEMS);

    // /* Main line properties */
    static lv_style_t main_line_style;
    lv_style_init(&main_line_style);
    lv_style_set_arc_color(&main_line_style, /*lv_color_black()*/ lv_color_hex(UI_COLOR_DIAL_FRAME));
    lv_style_set_arc_width(&main_line_style, 5);
    lv_obj_add_style(ui.dial, &main_line_style, LV_PART_MAIN);

    lv_scale_set_range(ui.dial, 0, 60);
    lv_scale_set_angle_range(ui.dial, 360);
    lv_scale_set_rotation(ui.dial, 270); // 0 = 3Uhr, 90=6Uhr, 180=9Uhr, 270=12Uhr

    ui.needleMM = mk_scale_needle(3, lv_color_hex(UI_COLOR_DIAL_NEEDLE_MM));
    ui.needleHH = mk_scale_needle(5, lv_color_hex(UI_COLOR_DIAL_NEEDLE_HH));
    ui.needleMM = mk_scale_needle_mutable(g_minute_hand_points, 3, lv_color_hex(UI_COLOR_DIAL_NEEDLE_MM));
    ui.needleHH = mk_scale_needle_mutable(g_hour_hand_points, 5, lv_color_hex(UI_COLOR_DIAL_NEEDLE_MM));

    // ui->countdown_minutes = 4 * 60 + 0;
    // ui->hours = ui->countdown_minutes / 60;
    // ui->minutes = ui->countdown_minutes % 60;
    // int32_t hour_pos = (ui->hours % 12) * 5 + (ui->minutes / 12);

    // lv_scale_set_line_needle_value(ui.dial, ui->needleMM, TIME_NEEDLE_LEN_M, ui.minutes);
    // lv_scale_set_line_needle_value(ui.dial, ui->needleHH, TIME_NEEDLE_LEN_H, hour_pos);

    // ----
    lv_obj_set_size(ui.dial, UI_DIAL_SIZE - 8, UI_DIAL_SIZE - 8);
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
    lv_obj_clear_flag(ui.start_button_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.start_button_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.start_button_container, LV_ALIGN_RIGHT_MID, -UI_FRAME_PADDING, 0);
    lv_obj_set_style_bg_opa(ui.start_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.start_button_container, LV_OPA_TRANSP, 0);

    ui.btn_start = lv_btn_create(ui.start_button_container);

    // Remove all theme styles so we have full control over the button appearance
    // lv_obj_remove_style_all(ui.btn_start);

    lv_obj_set_size(ui.btn_start, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);
    lv_obj_center(ui.btn_start);
    lv_obj_add_event_cb(ui.btn_start, start_button_event_cb, LV_EVENT_CLICKED, nullptr);

    // Base style for START button: solid color, no gradient
    // lv_obj_remove_style_all(ui.btn_start);
    lv_obj_set_style_radius(ui.btn_start, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.btn_start, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.btn_start, color_from_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);

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
    lv_obj_align(ui.bottom_container, LV_ALIGN_BOTTOM_MID, 0, UI_BOTTOM_PADDING + 10);

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
    lv_obj_set_size(ui.temp_indicator_current, 4, UI_TEMP_SCALE_HEIGHT);
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
    auto set_icon_state = [](lv_obj_t *obj, lv_color_t on_color, bool on)
    {
        if (!obj)
        {
            return;
        }

        if (on)
        {
            // Recolor the white icon to the given ON color
            lv_obj_set_style_img_recolor(obj, on_color, LV_PART_MAIN);
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        }
        else
        {
            // OFF: show original white icon (no recolor)
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
        }
    };

    const lv_color_t col_on = color_from_hex(UI_COLOR_ICON_ON_HEX);               // z.B. grün
    const lv_color_t col_door_open = color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX); // z.B. rot

    // 12V fan: white (off) -> green (on)
    set_icon_state(ui.icon_fan12v, col_on, state.fan12v_on);

    // 230V fan fast: white (off) -> green (on)
    set_icon_state(ui.icon_fan230, col_on, state.fan230_on);

    // 230V fan slow: white (off) -> green (on)
    set_icon_state(ui.icon_fan230_slow, col_on, state.fan230_slow_on);

    // Heater: white (off) -> green (on)
    set_icon_state(ui.icon_heater, col_on, state.heater_on);

    // Door: white (closed) -> red (open)
    set_icon_state(ui.icon_door, col_door_open, state.door_open);

    // Motor: white (off) -> green (on)
    set_icon_state(ui.icon_motor, col_on, state.motor_on);

    // Lamp: white (off) -> green (on)
    set_icon_state(ui.icon_lamp, col_on, state.lamp_on);

    // auto set_icon_color = [](lv_obj_t *obj, lv_color_t color)
    // {
    //     lv_obj_set_style_text_color(obj, color, 0);
    // };

    // // Base colors
    // const lv_color_t col_off = color_from_hex(UI_COLOR_ICON_OFF_HEX);
    // const lv_color_t col_on = color_from_hex(UI_COLOR_ICON_ON_HEX);
    // const lv_color_t col_door_open = color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX);

    // // 12V fan: white (off) -> green (on)
    // set_icon_color(ui.icon_fan12v, state.fan12v_on ? col_on : col_off);

    // // 230V fan: white (off) -> green (on)
    // set_icon_color(ui.icon_fan230, state.fan230_on ? col_on : col_off);

    // // 230V fan slow: white (off) -> green (on)
    // set_icon_color(ui.icon_fan230_slow, state.fan230_slow_on ? col_on : col_off);

    // // Heater: white (off) -> green (on)
    // set_icon_color(ui.icon_heater, state.heater_on ? col_on : col_off);

    // // Door: white (closed) -> red (open)
    // set_icon_color(ui.icon_door, state.door_open ? col_door_open : col_off);

    // // Motor: white (off) -> green (on)
    // set_icon_color(ui.icon_motor, state.motor_on ? col_on : col_off);

    // // Lamp: white (off) -> green (on)
    // set_icon_color(ui.icon_lamp, state.lamp_on ? col_on : col_off);
}

static void update_start_button_ui(void)
{
    if (!ui.btn_start || !ui.label_btn_start)
    {
        return;
    }

    const bool running = oven_is_running();

    if (running)
    {
        // Oven is running: button should be red and show STOP
        lv_label_set_text(ui.label_btn_start, "STOP");
        lv_obj_set_style_bg_color(ui.btn_start, color_from_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);
    }
    else
    {
        // Oven is stopped: button should be orange and show START
        lv_label_set_text(ui.label_btn_start, "START");
        lv_obj_set_style_bg_color(ui.btn_start, color_from_hex(0xFFA500), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);
    }
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

    if (oven_is_running())
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