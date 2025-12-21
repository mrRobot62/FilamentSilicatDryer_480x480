#include "screen_config.h"
#include "screen_base.h"
#include "../icons/icons_32x32.h"

// -----------------------------------------------------------------------------
// Forward declarations (local)
// -----------------------------------------------------------------------------
static void create_page_indicator(lv_obj_t *parent);
static void create_top_placeholder(lv_obj_t *parent);
static void create_bottom_placeholder(lv_obj_t *parent);

static void temp_roller_event_cb(lv_event_t *e);
static void hh_roller_event_cb(lv_event_t *e);
static void mm_roller_event_cb(lv_event_t *e);

static void apply_runtime_from_widgets(void);

static void create_buttons(lv_obj_t *parent);
static void btn_save_event_cb(lv_event_t *e);
static void btn_clear_event_cb(lv_event_t *e);

static void update_save_enabled(void);
static void save_state_timer_cb(lv_timer_t *t);

static void create_icons(lv_obj_t *parent);
static void icon_toggle_event_cb(lv_event_t *e);

// static void set_icon_visual(lv_obj_t *icon, bool on, bool special_red = false);
static void update_icons_enabled(void);
static void icons_state_timer_cb(lv_timer_t *t);
static void icon_set_state(lv_obj_t *img, bool enabled = false, bool manual_on = false);

static void icon_click_cb(lv_event_t *e);
static void icon_fan230_cb(lv_event_t *e);
static void icon_fan230_slow_cb(lv_event_t *e);
static void icon_heater_cb(lv_event_t *e);
static void icon_motor_cb(lv_event_t *e);
static void icon_lamp_cb(lv_event_t *e);

// runtime cache (for UI only)
static bool s_fan230 = false;
static bool s_fan230_slow = false;
static bool s_heater = false;
static bool s_motor = false;
static bool s_lamp = false;

// Prevent feedback loop when loading preset into rollers
static bool s_updating_widgets = false;
constexpr int LV_OPA_15 = 15;

// Icon colors for CONFIG screen
static constexpr uint32_t ICON_OFF_HEX = 0xFFFFFF;      // white
static constexpr uint32_t ICON_ON_HEX = 0xFF8A00;       // orange (manual override ON)
static constexpr uint32_t ICON_DISABLED_HEX = 0x707070; // gray

// -----------------------------------------------------------------------------
// PRIVATE Helpers
// -----------------------------------------------------------------------------
static void icon_set_color(lv_obj_t *img, uint32_t hex)
{
    if (!img)
        return;
    lv_obj_set_style_img_recolor(img, lv_color_hex(hex), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
}

static void icon_set_state(lv_obj_t *img, bool enabled, bool manual_on)
{
    if (!img)
        return;

    if (!enabled)
        icon_set_color(img, ICON_DISABLED_HEX);
    else if (manual_on)
        icon_set_color(img, ICON_ON_HEX);
    else
        icon_set_color(img, ICON_OFF_HEX);
}

// Creates an lv_img using the same pipeline as screen_main (PNG 32x32)
static lv_obj_t *create_icon_img(lv_obj_t *parent, const lv_img_dsc_t *dsc)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, dsc);

    // Keep consistent with screen_main icon behavior:
    // recolor controls the displayed color
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, lv_color_hex(ICON_OFF_HEX), 0);

    return img;
}

// static void update_save_enabled(void)
// {
//     if (!ui_config.btn_save)
//         return;

//     const bool can_save = !oven_is_running();
//     if (can_save)
//         lv_obj_clear_state(ui_config.btn_save, LV_STATE_DISABLED);
//     else
//         lv_obj_add_state(ui_config.btn_save, LV_STATE_DISABLED);
// }

static void set_roller_value_silent(lv_obj_t *roller, int value)
{
    // Using animation off reduces flicker
    lv_roller_set_selected(roller, value, LV_ANIM_OFF);
}

static void load_preset_to_widgets(int preset_index)
{
    const FilamentPreset *p = oven_get_preset(preset_index);
    s_updating_widgets = true;
    if (!p)
        return;

    // Temperature: temp roller has options starting at 20
    int temp = (int)p->dryTempC;
    if (temp < 20)
        temp = 20;
    if (temp > 120)
        temp = 120;
    set_roller_value_silent(ui_config.roller_drying_temp, temp - 20);

    // Time: assume preset minutes
    int minutes = (int)p->durationMin;
    if (minutes < 0)
        minutes = 0;
    int hh = minutes / 60;
    int mm = minutes % 60;

    if (hh > 24)
        hh = 24;
    set_roller_value_silent(ui_config.roller_time_hh, hh);

    // mm roller is 0..55 in 5-min steps
    int mm5 = (mm + 2) / 5; // nearest
    if (mm5 < 0)
        mm5 = 0;
    if (mm5 > 11)
        mm5 = 11;
    set_roller_value_silent(ui_config.roller_time_mm, mm5);

    // Bottom info message (optional)
    if (ui_config.label_info_message)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Loaded preset: %s", p->name);
        lv_label_set_text(ui_config.label_info_message, buf);
    }
    s_updating_widgets = false;
}

static void update_icons_enabled(void)
{
    const bool enabled = !oven_is_running();

    lv_obj_t *toggles[] = {
        ui_config.icon_fan230,
        ui_config.icon_fan230_slow,
        ui_config.icon_heater,
        ui_config.icon_motor,
        ui_config.icon_lamp};

    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); ++i)
    {
        if (!toggles[i])
            continue;
        if (enabled)
            lv_obj_clear_state(toggles[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(toggles[i], LV_STATE_DISABLED);
    }
}

static void update_icon_enable_state(void)
{
    const bool enabled = !oven_is_running();

    // Toggleables: if running => disabled gray, and clicks ignored
    icon_set_state(ui_config.icon_fan230, enabled, s_fan230);
    icon_set_state(ui_config.icon_fan230_slow, enabled, s_fan230_slow);
    icon_set_state(ui_config.icon_heater, enabled, s_heater);
    icon_set_state(ui_config.icon_motor, enabled, s_motor);
    icon_set_state(ui_config.icon_lamp, enabled, s_lamp);

    // Always-disabled display icons
    icon_set_state(ui_config.icon_fan12v, false, false);
    icon_set_state(ui_config.icon_door, false, false);
}

static void update_icon_enabled(void)
{
    const bool enable_toggles = !oven_is_running();

    // fan12v and door are always disabled in config
    if (ui_config.icon_fan12v)
        lv_obj_clear_flag(ui_config.icon_fan12v, LV_OBJ_FLAG_CLICKABLE);
    if (ui_config.icon_door)
        lv_obj_clear_flag(ui_config.icon_door, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *toggles[] = {
        ui_config.icon_fan230,
        ui_config.icon_fan230_slow,
        ui_config.icon_heater,
        ui_config.icon_motor,
        ui_config.icon_lamp};

    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); ++i)
    {
        if (!toggles[i])
            continue;

        if (enable_toggles)
            lv_obj_clear_state(toggles[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(toggles[i], LV_STATE_DISABLED);
    }
}

static void update_icon_colors(void)
{
    const bool enable_toggles = !oven_is_running();

    // disabled ones always gray
    icon_set_color(ui_config.icon_fan12v, ICON_DISABLED_HEX);
    icon_set_color(ui_config.icon_door, ICON_DISABLED_HEX);

    // toggleables: gray when disabled, else OFF/ON
    auto color_for = [&](bool on) -> uint32_t
    {
        if (!enable_toggles)
            return ICON_DISABLED_HEX;
        return on ? ICON_ON_HEX : ICON_OFF_HEX;
    };

    icon_set_color(ui_config.icon_fan230, color_for(s_fan230));
    icon_set_color(ui_config.icon_fan230_slow, color_for(s_fan230_slow));
    icon_set_color(ui_config.icon_heater, color_for(s_heater));
    icon_set_color(ui_config.icon_motor, color_for(s_motor));
    icon_set_color(ui_config.icon_lamp, color_for(s_lamp));
}

// -----------------------------------------------------------------------------
// CREATE Config Screen UI
// -----------------------------------------------------------------------------
static void create_config_rollers(lv_obj_t *parent)
{
    // Layout: simple vertical stack inside middle container
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 10, 0);

    // ---------- Filament preset roller ----------
    ui_config.roller_filament_type = lv_roller_create(parent);
    lv_obj_set_width(ui_config.roller_filament_type, 340);
    lv_roller_set_visible_row_count(ui_config.roller_filament_type, 4);

    // Build roller options from presets
    // Assumption: oven exposes preset list/count OR you have constants in oven.h.
    // We'll use two functions you should already have or can add easily:
    //   int oven_preset_count(void);
    //   const FilamentPreset* oven_preset_get(int idx);
    //
    // If you don't have them yet, we can stub them next step, but try this first.
    const int n = oven_get_preset_count();
    static char opts[1024];
    opts[0] = '\0';

    for (int i = 0; i < n; ++i)
    {
        const FilamentPreset *p = oven_get_preset(i);
        if (!p)
            continue;

        // One line per preset
        char line[64];
        std::snprintf(line, sizeof(line), "%s\n", p->name);
        std::strncat(opts, line, sizeof(opts) - std::strlen(opts) - 1);
    }

    // Remove trailing newline if present (LVGL tolerates both, but clean is nice)
    size_t len = std::strlen(opts);
    if (len > 0 && opts[len - 1] == '\n')
        opts[len - 1] = '\0';

    lv_roller_set_options(ui_config.roller_filament_type, opts, LV_ROLLER_MODE_NORMAL);
    lv_obj_add_event_cb(ui_config.roller_filament_type, filament_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ---------- Temperature roller ----------
    ui_config.roller_drying_temp = lv_roller_create(parent);
    lv_obj_set_width(ui_config.roller_drying_temp, 340);
    lv_roller_set_visible_row_count(ui_config.roller_drying_temp, 3);

    // Temp options 20..120 (1°C steps) – adjust later if you want
    static char temp_opts[1024];
    temp_opts[0] = '\0';
    for (int t = 20; t <= 120; ++t)
    {
        char line[8];
        std::snprintf(line, sizeof(line), "%d\n", t);
        std::strncat(temp_opts, line, sizeof(temp_opts) - std::strlen(temp_opts) - 1);
    }
    len = std::strlen(temp_opts);
    if (len > 0 && temp_opts[len - 1] == '\n')
        temp_opts[len - 1] = '\0';
    lv_roller_set_options(ui_config.roller_drying_temp, temp_opts, LV_ROLLER_MODE_NORMAL);

    // ---------- Time HH roller ----------
    ui_config.roller_time_hh = lv_roller_create(parent);
    lv_obj_set_width(ui_config.roller_time_hh, 160);
    lv_roller_set_visible_row_count(ui_config.roller_time_hh, 3);

    static char hh_opts[256];
    hh_opts[0] = '\0';
    for (int h = 0; h <= 24; ++h)
    {
        char line[8];
        std::snprintf(line, sizeof(line), "%02d\n", h);
        std::strncat(hh_opts, line, sizeof(hh_opts) - std::strlen(hh_opts) - 1);
    }
    len = std::strlen(hh_opts);
    if (len > 0 && hh_opts[len - 1] == '\n')
        hh_opts[len - 1] = '\0';
    lv_roller_set_options(ui_config.roller_time_hh, hh_opts, LV_ROLLER_MODE_NORMAL);

    // ---------- Time MM roller ----------
    ui_config.roller_time_mm = lv_roller_create(parent);
    lv_obj_set_width(ui_config.roller_time_mm, 160);
    lv_roller_set_visible_row_count(ui_config.roller_time_mm, 3);

    static char mm_opts[128];
    mm_opts[0] = '\0';
    for (int m = 0; m <= 55; m += 5)
    {
        char line[8];
        std::snprintf(line, sizeof(line), "%02d\n", m);
        std::strncat(mm_opts, line, sizeof(mm_opts) - std::strlen(mm_opts) - 1);
    }
    len = std::strlen(mm_opts);
    if (len > 0 && mm_opts[len - 1] == '\n')
        mm_opts[len - 1] = '\0';
    lv_roller_set_options(ui_config.roller_time_mm, mm_opts, LV_ROLLER_MODE_NORMAL);

    // Optional: preload currently active preset (if you have an API)
    // Otherwise default to preset 0 for now.
    int idx = oven_get_current_preset_index(); // if you have it
    if (idx < 0 || idx >= n)
        idx = 0;

    // Event callbacks
    lv_obj_add_event_cb(ui_config.roller_drying_temp, temp_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_config.roller_time_hh, hh_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_config.roller_time_mm, mm_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    set_roller_value_silent(ui_config.roller_filament_type, idx);
    load_preset_to_widgets(idx);
}

static void create_buttons(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 12, 0);

    // SAVE
    ui_config.btn_save = lv_btn_create(parent);
    lv_obj_set_size(ui_config.btn_save, 120, 60);
    ui_config.label_btn_save = lv_label_create(ui_config.btn_save);
    lv_label_set_text(ui_config.label_btn_save, "SAVE");
    lv_obj_center(ui_config.label_btn_save);
    lv_obj_add_event_cb(ui_config.btn_save, btn_save_event_cb, LV_EVENT_CLICKED, NULL);

    // CLEAR
    ui_config.btn_clear = lv_btn_create(parent);
    lv_obj_set_size(ui_config.btn_clear, 120, 60);
    ui_config.label_btn_clear = lv_label_create(ui_config.btn_clear);
    lv_label_set_text(ui_config.label_btn_clear, "CLEAR");
    lv_obj_center(ui_config.label_btn_clear);
    lv_obj_add_event_cb(ui_config.btn_clear, btn_clear_event_cb, LV_EVENT_CLICKED, NULL);
}

static void create_icons(lv_obj_t *parent)
{
    ui_config.icons_container = lv_obj_create(ui_config.center_container);
    lv_obj_clear_flag(ui_config.icons_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_config.icons_container, UI_SIDE_PADDING, 300);
    lv_obj_align(ui_config.icons_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui_config.icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui_config.icons_container, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(ui_config.icons_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_config.icons_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // // Same container behavior as screen_main
    // lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    // lv_obj_set_style_border_opa(parent, LV_OPA_TRANSP, 0);

    // lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    // lv_obj_set_flex_align(parent,
    //                       LV_FLEX_ALIGN_CENTER,
    //                       LV_FLEX_ALIGN_CENTER,
    //                       LV_FLEX_ALIGN_CENTER);

    // --- Create 32x32 icons (same sources as screen_main) ---
    ui_config.icon_fan12v = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_fan12v, &fan12v_wht);
    lv_obj_set_size(ui_config.icon_fan12v, 32, 32);

    ui_config.icon_fan230 = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_fan230, &fan230v_fast_wht);
    lv_obj_set_size(ui_config.icon_fan230, 32, 32);
    lv_obj_add_flag(ui_config.icon_fan230, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_fan230, icon_fan230_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_fan230_slow = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_fan230_slow, &fan230v_low_wht);
    lv_obj_set_size(ui_config.icon_fan230_slow, 32, 32);
    lv_obj_add_flag(ui_config.icon_fan230_slow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_fan230_slow, icon_fan230_slow_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_heater = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_heater, &heater_wht);
    lv_obj_set_size(ui_config.icon_heater, 32, 32);
    lv_obj_add_flag(ui_config.icon_heater, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_heater, icon_heater_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_door = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_door, &door_open_wht);
    lv_obj_set_size(ui_config.icon_door, 32, 32);
    // Door is display-only in config (no click handler)

    ui_config.icon_motor = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_motor, &motor230v);
    lv_obj_set_size(ui_config.icon_motor, 32, 32);
    lv_obj_add_flag(ui_config.icon_motor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_motor, icon_motor_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_lamp = lv_image_create(ui_config.icons_container);
    lv_image_set_src(ui_config.icon_lamp, &lamp230v_wht);
    lv_obj_set_size(ui_config.icon_lamp, 32, 32);
    lv_obj_add_flag(ui_config.icon_lamp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_lamp, icon_lamp_cb, LV_EVENT_CLICKED, nullptr);

    // Initial state: fan12v + door disabled, others OFF (white)
    update_icon_enabled();
    update_icon_colors();
}

// -----------------------------------------------------------------------------
// Event-Callback
// -----------------------------------------------------------------------------
static void update_save_enabled(void)
{
    if (!ui_config.btn_save)
        return;

    const bool can_save = !oven_is_running();
    if (can_save)
        lv_obj_clear_state(ui_config.btn_save, LV_STATE_DISABLED);
    else
        lv_obj_add_state(ui_config.btn_save, LV_STATE_DISABLED);
}

static void save_state_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_save_enabled();
}

static void btn_save_event_cb(lv_event_t *e)
{
    (void)e;

    if (oven_is_running())
    {
        UI_WARN("[screen_config] SAVE blocked (oven running)\n");
        return;
    }

    UI_INFO("[screen_config] SAVE -> go home\n");
    if (ui_config.label_info_message)
        lv_label_set_text(ui_config.label_info_message, "Saved (runtime) - returning...");

    screen_manager_go_home();
}

static void btn_clear_event_cb(lv_event_t *e)
{
    (void)e;
    UI_INFO("[screen_config] CLEAR\n");

    // Neutral values
    s_updating_widgets = true;
    set_roller_value_silent(ui_config.roller_filament_type, 0);
    set_roller_value_silent(ui_config.roller_drying_temp, 0); // -> 20C
    set_roller_value_silent(ui_config.roller_time_hh, 0);
    set_roller_value_silent(ui_config.roller_time_mm, 0);
    s_updating_widgets = false;

    // Apply neutral runtime
    oven_set_runtime_temp_target(20);
    oven_set_runtime_duration_minutes(0);

    if (ui_config.label_info_message)
        lv_label_set_text(ui_config.label_info_message, "Cleared (runtime)");

    s_fan230 = s_fan230_slow = s_heater = s_motor = s_lamp = false;
    icon_set_state(ui_config.icon_fan230, false);
    icon_set_state(ui_config.icon_fan230_slow, false);
    icon_set_state(ui_config.icon_heater, false);
    icon_set_state(ui_config.icon_motor, false);
    icon_set_state(ui_config.icon_lamp, false);

    oven_set_runtime_actuator_fan230(false);

    oven_set_runtime_actuator_fan230_slow(false);
    oven_set_runtime_actuator_heater(false);
    oven_set_runtime_actuator_motor(false);
    oven_set_runtime_actuator_lamp(false);

    s_fan230 = s_fan230_slow = s_heater = s_motor = s_lamp = false;
    update_icon_enable_state();
}

static void icons_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_icon_enabled();
    update_icon_colors();
}

static void filament_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    const int idx = lv_roller_get_selected(roller);

    UI_INFO("[screen_config] preset selected index=%d\n", idx);
    load_preset_to_widgets(idx);
}

static void temp_roller_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_updating_widgets)
        return;
    apply_runtime_from_widgets();
}

static void hh_roller_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_updating_widgets)
        return;
    apply_runtime_from_widgets();
}

static void mm_roller_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_updating_widgets)
        return;
    apply_runtime_from_widgets();
}

static void icon_toggle_event_cb(lv_event_t *e)
{
    if (oven_is_running())
    {
        UI_WARN("[screen_config] icon toggle blocked (oven running)\n");
        return;
    }

    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    const int id = (int)(intptr_t)lv_event_get_user_data(e);

    switch (id)
    {
    case 1: // fan230
        s_fan230 = !s_fan230;
        icon_set_state(ui_config.icon_fan230, s_fan230);
        oven_set_runtime_actuator_fan230(s_fan230);
        break;

    case 2: // fan230 slow
        s_fan230_slow = !s_fan230_slow;
        icon_set_state(ui_config.icon_fan230_slow, s_fan230_slow);
        oven_set_runtime_actuator_fan230_slow(s_fan230_slow);
        break;

    case 3: // heater
        s_heater = !s_heater;
        icon_set_state(ui_config.icon_heater, s_heater);
        oven_set_runtime_actuator_heater(s_heater);
        break;

    case 4: // motor
        s_motor = !s_motor;
        icon_set_state(ui_config.icon_motor, s_motor);
        oven_set_runtime_actuator_motor(s_motor);
        break;

    case 5: // lamp
        s_lamp = !s_lamp;
        icon_set_state(ui_config.icon_lamp, s_lamp);
        oven_set_runtime_actuator_lamp(s_lamp);
        break;

    default:
        break;
    }

    (void)target;
}

static void icon_fan230_cb(lv_event_t *e)
{
    (void)e;
    if (oven_is_running())
        return;
    s_fan230 = !s_fan230;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_fan230(s_fan230);
}

static void icon_fan230_slow_cb(lv_event_t *e)
{
    (void)e;
    if (oven_is_running())
        return;
    s_fan230_slow = !s_fan230_slow;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_fan230_slow(s_fan230_slow);
}

static void icon_heater_cb(lv_event_t *e)
{
    (void)e;
    if (oven_is_running())
        return;
    s_heater = !s_heater;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_heater(s_heater);
}

static void icon_motor_cb(lv_event_t *e)
{
    (void)e;
    if (oven_is_running())
        return;
    s_motor = !s_motor;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_motor(s_motor);
}

static void icon_lamp_cb(lv_event_t *e)
{
    (void)e;
    if (oven_is_running())
        return;
    s_lamp = !s_lamp;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_lamp(s_lamp);
}

static void icons_state_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_icons_enabled();
}
// -----------------------------------------------------------------------------
// HELPERS
// -----------------------------------------------------------------------------

static void apply_runtime_from_widgets(void)
{
    // Temperature roller is 20..120 mapped by index 0..100
    const int temp_idx = lv_roller_get_selected(ui_config.roller_drying_temp);
    const int temp_c = 20 + temp_idx;

    const int hh = lv_roller_get_selected(ui_config.roller_time_hh);

    // MM roller uses 0..55 in 5-min steps => idx 0..11
    const int mm5 = lv_roller_get_selected(ui_config.roller_time_mm);
    const int mm = mm5 * 5;

    const int duration_min = hh * 60 + mm;

    // Apply to runtime only (no persistence)
    oven_set_runtime_temp_target((uint16_t)temp_c);
    oven_set_runtime_duration_minutes((uint16_t)duration_min);

    if (ui_config.label_info_message)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Runtime: %dC / %02d:%02d", temp_c, hh, mm);
        lv_label_set_text(ui_config.label_info_message, buf);
    }

    UI_INFO("[screen_config] runtime updated: temp=%dC duration=%dmin\n", temp_c, duration_min);
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
lv_obj_t *screen_config_create(lv_obj_t *parent)
{
    // Return existing instance
    if (ui_config.root != nullptr)
    {
        UI_INFO("[screen_config] reusing existing root\n");
        return ui_config.root;
    }

    // Build base layout (480x480)
    ScreenBaseLayout base{};
    screen_base_create(&base, parent,
                       UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT,
                       60, 60, 60, // top_h, page_h, bottom_h
                       60, 360);   // side_w, center_w

    // ----------- ASSIGN containers
    // Assign base containers into ui_config
    ui_config.root = base.root;

    ui_config.top_bar_container = base.top;
    ui_config.center_container = base.center;
    ui_config.page_indicator_container = base.page_indicator;
    ui_config.bottom_container = base.bottom;

    ui_config.icons_container = base.left;
    ui_config.config_container = base.middle;
    ui_config.button_container = base.right;

    // ----------- CREATE UI PARTS
    create_page_indicator(ui_config.page_indicator_container);
    create_config_rollers(ui_config.config_container);
    create_buttons(ui_config.button_container);

    // Update SAVE enabled state periodically (running may change)
    lv_timer_create(save_state_timer_cb, 250, NULL);
    update_save_enabled();

    // create_icons(ui_config.icons_container);

    // // Update icon enabled state periodically (running may change)
    // lv_timer_create(icons_state_timer_cb, 250, NULL);
    // update_icons_enabled();

    create_icons(ui_config.icons_container);
    lv_timer_create(icons_state_timer_cb, 250, NULL);
    update_icons_enabled();
    update_icon_colors();

    // ----------- PLACEHOLDERS
    // Simple placeholders so the screen is clearly visible
    create_top_placeholder(ui_config.top_bar_container);
    create_bottom_placeholder(ui_config.bottom_container);

    // Start a timer to update Save button state periodically
    lv_timer_create(save_state_timer_cb, 250, nullptr);

    UI_INFO("[screen_config] created root=%p swipe_target=%p\n",
            (void *)ui_config.root, (void *)ui_config.s_swipe_target);

    return ui_config.root;
}

lv_obj_t *screen_config_get_swipe_target(void)
{
    return ui_config.s_swipe_target;
}

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static void create_page_indicator(lv_obj_t *parent)
{
    // Make the whole page-indicator container the swipe target
    ui_config.s_swipe_target = parent;

    // Must be clickable to receive pointer events
    lv_obj_add_flag(ui_config.s_swipe_target, LV_OBJ_FLAG_CLICKABLE);

    // Very subtle hint so user knows where to swipe
    // (you can tune LV_OPA_4..LV_OPA_10)
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_15, 0);
    lv_obj_set_style_radius(parent, 10, 0);

    // Inner panel (rounded rectangle) + dots (optional visual consistency)
    ui_config.page_indicator_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_config.page_indicator_panel);
    lv_obj_set_size(ui_config.page_indicator_panel, 100, 24);
    lv_obj_center(ui_config.page_indicator_panel);

    lv_obj_set_style_radius(ui_config.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui_config.page_indicator_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(ui_config.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_config.page_indicator_panel, 4, 0);

    lv_obj_set_flex_flow(ui_config.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_config.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        ui_config.page_dots[i] = lv_obj_create(ui_config.page_indicator_panel);
        lv_obj_remove_style_all(ui_config.page_dots[i]);
        lv_obj_set_size(ui_config.page_dots[i], 10, 10);
        lv_obj_set_style_radius(ui_config.page_dots[i], 5, 0);
        lv_obj_set_style_bg_opa(ui_config.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui_config.page_dots[i],
                                  (i == 1) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x606060), 0);
        if (i > 0)
            lv_obj_set_style_margin_left(ui_config.page_dots[i], 8, 0);
    }
}

static void create_top_placeholder(lv_obj_t *parent)
{
    // Just a title so we see we're on CONFIG
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "CONFIG");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

static void create_bottom_placeholder(lv_obj_t *parent)
{
    // Bottom info message placeholder
    ui_config.label_info_message = lv_label_create(parent);
    lv_label_set_text(ui_config.label_info_message, "Swipe here to change screens");
    lv_obj_set_style_text_color(ui_config.label_info_message, lv_color_hex(0xB0B0B0), 0);
    lv_obj_center(ui_config.label_info_message);
}