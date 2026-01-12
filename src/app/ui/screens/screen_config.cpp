#include "screen_config.h"
#include "../icons/icons_32x32.h"
#include "screen_base.h"

#define CFG_STAGE 60

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

static constexpr int kRollerH = 108;    // common roller height (tune later)
static constexpr int kRollerTimeW = 55; // width for HH/MM rollers
static constexpr int kRollerTempW = 75; // width for HH/MM rollers

static bool s_screen_ready = false;

// Common geometry
static constexpr int kCardW_Fil = 340;
static constexpr int kCardW_Small = 165;
static constexpr int kCardH = 152; // tuned for title + 3-row roller
static constexpr int kZoneB_H = kCardH;

// general Page geometry
static constexpr int BASE_TOP_H = 40;
static constexpr int BASE_BOTTOM_H = 60;
static constexpr int BASE_PAGE_INDICATOR_H = 40;
static constexpr int BASE_SIDE_W = 60;
static constexpr int BASE_CENTER_H = (480 - BASE_TOP_H - BASE_BOTTOM_H - BASE_PAGE_INDICATOR_H);
static constexpr int BASE_CENTER_W = 480 - 2 * BASE_SIDE_W;

// -----------------------------------------------------------------------------
// PRIVATE Helpers
// -----------------------------------------------------------------------------

static void style_roller_green(lv_obj_t *roller) {
    // Selected row background (green)
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x00A000), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);

    // Selected text
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_text_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
}

// Rounded "card" with title label (FILAMENT / TIME / TEMP)

static void dump_obj(const char *tag, lv_obj_t *o) {
    if (!o) {
        UI_INFO("%s: <null>\n", tag);
        return;
    }
    UI_INFO("SIZE from %s: obj=%p valid=%d w=%d h=%d\n",
            tag, (void *)o, lv_obj_is_valid(o),
            (int)lv_obj_get_width(o), (int)lv_obj_get_height(o));

    lv_area_t a;
    lv_obj_get_content_coords(o, &a);
    UI_INFO("Coords from %s: content_coords: x1=%d y1=%d x2=%d y2=%d cw=%d ch=%d\n",
            tag, (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2,
            (int)(a.x2 - a.x1 + 1), (int)(a.y2 - a.y1 + 1));
}

static lv_obj_t *create_card(lv_obj_t *parent, int w, int h, const char *title, lv_obj_t **out_content, lv_text_align_t lblAlign = LV_TEXT_ALIGN_CENTER) {
    if (out_content) {
        *out_content = nullptr;
    }

    UI_INFO("[CFG] card '%s' parent=%p w=%d h=%d\n", title, (void *)parent, w, h);
    if (!parent) {
        UI_ERR("[CFG] card '%s' parent NULL!\n", title);
        return nullptr;
    }
    lv_obj_t *card = lv_obj_create(parent);
    if (!card) {
        UI_ERR("[screen_config] create_card: lv_obj_create(card) failed for '%s'\n", title);
        return nullptr;
    }
    UI_INFO("[CFG] card '%s' card=%p\n", title, (void *)card);

    if (!parent) {
        UI_ERR("[CFG] card '%s' parent NULL!\n", title);
        return nullptr;
    }
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, w, h);

    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x404040), 0);

    lv_obj_set_style_bg_color(card, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_set_style_pad_left(card, 10, 0);
    lv_obj_set_style_pad_right(card, 10, 0);
    lv_obj_set_style_pad_top(card, 10, 0);
    lv_obj_set_style_pad_bottom(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, lblAlign, 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_70, 0);
    UI_INFO("[CFG] card '%s' lbl=%p\n", title, (void *)lbl);

    // Content container (centered block under title)
    lv_obj_t *content = lv_obj_create(card);
    if (!content) {
        UI_ERR("[screen_config] create_card: lv_obj_create(content) failed for '%s'\n", title);
        // Fallback: caller can attach children directly to card
        if (out_content) {
            *out_content = card;
        }
        return card;
    }
    UI_INFO("[CFG] card '%s' content=%p\n", title, (void *)content);

    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_grow(content, 1);

    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (out_content) {
        *out_content = content;
    }
    return card;
}

static void icon_set_color(lv_obj_t *img, uint32_t hex) {
    if (!img) {
        return;
    }
    lv_obj_set_style_img_recolor(img, lv_color_hex(hex), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
}

static void icon_set_state(lv_obj_t *img, bool enabled, bool manual_on) {
    if (!img) {
        return;
    }

    if (!enabled) {
        icon_set_color(img, ICON_DISABLED_HEX);
    } else if (manual_on) {
        icon_set_color(img, ICON_ON_HEX);
    } else {
        icon_set_color(img, ICON_OFF_HEX);
    }
}

// Creates an lv_img using the same pipeline as screen_main (PNG 32x32)
static lv_obj_t *create_icon_img(lv_obj_t *parent, const lv_img_dsc_t *dsc) {
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, dsc);

    // Keep consistent with screen_main icon behavior:
    // recolor controls the displayed color
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, lv_color_hex(ICON_OFF_HEX), 0);

    return img;
}

static void set_roller_value_silent(lv_obj_t *roller, int value) {
    // Using animation off reduces flicker
    lv_roller_set_selected(roller, value, LV_ANIM_OFF);
}

static void load_preset_to_widgets(int preset_index) {
    const FilamentPreset *p = oven_get_preset(preset_index);
    // If stage/testing hasn't created these widgets yet, do nothing.
    if (!ui_config.roller_drying_temp || !ui_config.roller_time_hh || !ui_config.roller_time_mm) {
        UI_WARN("[CFG] load_preset_to_widgets skipped (rollers not created)\n");
        return;
    }

    s_updating_widgets = true;
    if (!p) {
        s_updating_widgets = false;
        return;
    }

    // Temperature: temp roller has options starting at 20
    int temp = (int)p->dryTempC;
    if (temp < 0) {
        temp = 0;
    }
    if (temp > 120) {
        temp = 120;
    }
    set_roller_value_silent(ui_config.roller_drying_temp, temp);

    // Time: assume preset minutes
    int minutes = (int)p->durationMin;
    if (minutes < 0) {
        minutes = 0;
    }
    int hh = minutes / 60;
    int mm = minutes % 60;

    if (hh > 24) {
        hh = 24;
    }
    set_roller_value_silent(ui_config.roller_time_hh, hh);

    // mm roller is 0..55 in 5-min steps
    int mm5 = (mm + 2) / 5; // nearest
    if (mm5 < 0) {
        mm5 = 0;
    }
    if (mm5 > 11) {
        mm5 = 11;
    }
    set_roller_value_silent(ui_config.roller_time_mm, mm5);

    // Bottom info message (optional)
    if (ui_config.label_info_message) {
        char buf[64];
        //        std::snprintf(buf, sizeof(buf), "Loaded preset: %s", p->name);
        std::snprintf(buf, sizeof(buf), "Loaded preset '%s' %02d:%02d with %3d°C to widgets\n", p->name, hh, mm5, temp);
        lv_label_set_text(ui_config.label_info_message, buf);
        UI_INFO(buf);
    }
    s_updating_widgets = false;
}

static void update_icons_enabled(void) {
    const bool enabled = !oven_is_running();

    lv_obj_t *toggles[] = {
        ui_config.icon_fan230,
        ui_config.icon_fan230_slow,
        ui_config.icon_heater,
        ui_config.icon_motor,
        ui_config.icon_lamp};

    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); ++i) {
        if (!toggles[i]) {
            continue;
        }
        if (enabled) {
            lv_obj_clear_state(toggles[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(toggles[i], LV_STATE_DISABLED);
        }
    }
}

static void update_icon_enable_state(void) {
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

static void update_icon_enabled(void) {
    const bool enable_toggles = !oven_is_running();

    // fan12v and door are always disabled in config
    if (ui_config.icon_fan12v) {
        lv_obj_clear_flag(ui_config.icon_fan12v, LV_OBJ_FLAG_CLICKABLE);
    }
    if (ui_config.icon_door) {
        lv_obj_clear_flag(ui_config.icon_door, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *toggles[] = {
        ui_config.icon_fan230,
        ui_config.icon_fan230_slow,
        ui_config.icon_heater,
        ui_config.icon_motor,
        ui_config.icon_lamp};

    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); ++i) {
        if (!toggles[i]) {
            continue;
        }

        if (enable_toggles) {
            lv_obj_clear_state(toggles[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(toggles[i], LV_STATE_DISABLED);
        }
    }
}

static void update_icon_colors(void) {
    const bool enable_toggles = !oven_is_running();

    // disabled ones always gray
    icon_set_color(ui_config.icon_fan12v, ICON_DISABLED_HEX);
    icon_set_color(ui_config.icon_door, ICON_DISABLED_HEX);

    // toggleables: gray when disabled, else OFF/ON
    auto color_for = [&](bool on) -> uint32_t {
        if (!enable_toggles) {
            return ICON_DISABLED_HEX;
        }
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
static void create_config_rollers(lv_obj_t *parent) {

    // Root layout for middle container
    // lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    // - only debug
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(parent, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    // lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Fix top clipping + spacing between groups
    // lv_obj_set_style_pad_top(parent, 4, 0);    // war 4
    // lv_obj_set_style_pad_left(parent, 4, 0);   // NEU: wichtig!
    // lv_obj_set_style_pad_bottom(parent, 4, 0); // NEU: wichtig!
    lv_obj_set_style_pad_all(parent, 8, 0); // NEU: wichtig!
    lv_obj_set_style_pad_row(parent, 6, 0); // war 4 (klein bisschen mehr Luft)

    // ------------------------------------------------------------
    // Card A: FILAMENT
    // ------------------------------------------------------------
    lv_obj_t *fil_content = nullptr;
    lv_obj_t *card_fil = create_card(parent, kCardW_Fil, kCardH, "FILAMENT", &fil_content);
    if (!fil_content) {
        fil_content = card_fil;
    }

    ui_config.roller_filament_type = lv_roller_create(fil_content);
    lv_obj_set_width(ui_config.roller_filament_type, 340);
    lv_obj_set_height(ui_config.roller_filament_type, kRollerH);
    lv_roller_set_visible_row_count(ui_config.roller_filament_type, 3);
    style_roller_green(ui_config.roller_filament_type);

    // Build roller options from presets
    const int n = oven_get_preset_count();
    static char opts[2048];
    opts[0] = '\0';

    for (int i = 0; i < n; ++i) {
        const FilamentPreset *p = oven_get_preset(i);
        if (!p) {
            continue;
        }

        char line[64];
        std::snprintf(line, sizeof(line), "%s\n", p->name);
        std::strncat(opts, line, sizeof(opts) - std::strlen(opts) - 1);
    }

    size_t len = std::strlen(opts);
    if (len > 0 && opts[len - 1] == '\n') {
        opts[len - 1] = '\0';
    }

    lv_roller_set_options(ui_config.roller_filament_type, opts, LV_ROLLER_MODE_NORMAL);
    lv_obj_add_event_cb(ui_config.roller_filament_type, filament_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ------------------------------------------------------------
    // Zone B: TIME + TEMP row  (STAGE 50..60)  -- NO time_row
    // ------------------------------------------------------------
    // 50: create zone_b only
    lv_obj_t *zone_b = lv_obj_create(parent);
    lv_obj_remove_style_all(zone_b);
    lv_obj_clear_flag(zone_b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(zone_b, BASE_CENTER_W, kZoneB_H);

    lv_obj_set_flex_flow(zone_b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(zone_b,
                          //                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // 51: create TIME card only
    lv_obj_t *time_content = nullptr;
    lv_obj_t *card_time = create_card(
        zone_b,
        kCardW_Small,
        kCardH,
        "TIME HH:MM",
        &time_content,
        LV_TEXT_ALIGN_CENTER);
    if (!time_content) {
        time_content = card_time;
    }

    // Kill Flex for content; we place children manually
    lv_obj_clear_flag(time_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(time_content, LV_LAYOUT_NONE);
    lv_obj_set_size(time_content, LV_PCT(100), kRollerH);

    // 52: (legacy) time_row removed -> layout update / sanity only
    lv_obj_update_layout(parent);
    lv_obj_update_layout(zone_b);
    lv_obj_update_layout(card_time);
    lv_obj_update_layout(time_content);

#if (CFG_STAGE == 52)
    return;
#endif

    // 53: create HH roller (NO options)
    ui_config.roller_time_hh = lv_roller_create(time_content);
    lv_obj_set_size(ui_config.roller_time_hh, kRollerTimeW, kRollerH);
    lv_roller_set_visible_row_count(ui_config.roller_time_hh, 3);

    // manual placement (left)
    lv_obj_align(ui_config.roller_time_hh, LV_ALIGN_LEFT_MID, 0, 0);

#if (CFG_STAGE == 53)
    return;
#endif

    // 54: style HH roller + basic placement params (no flex)
    style_roller_green(ui_config.roller_time_hh);

#if (CFG_STAGE == 54)
    return;
#endif

    // 55: set HH options
    static char hh_opts[256];
    hh_opts[0] = '\0';
    for (int h = 0; h <= 24; ++h) {
        char line[8];
        std::snprintf(line, sizeof(line), "%02d\n", h);
        std::strncat(hh_opts, line, sizeof(hh_opts) - std::strlen(hh_opts) - 1);
    }
    size_t len_hh = std::strlen(hh_opts);
    if (len_hh > 0 && hh_opts[len_hh - 1] == '\n') {
        hh_opts[len_hh - 1] = '\0';
    }
    lv_roller_set_options(ui_config.roller_time_hh, hh_opts, LV_ROLLER_MODE_NORMAL);

#if (CFG_STAGE == 55)
    return;
#endif

    // 56: add colon (center between rollers)
    lv_obj_t *lbl_colon = lv_label_create(time_content);
    lv_label_set_text(lbl_colon, ":");
    lv_obj_set_style_text_color(lbl_colon, lv_color_white(), 0);
    lv_obj_set_style_text_opa(lbl_colon, LV_OPA_90, 0);

    // place colon right after HH roller
    lv_obj_align_to(lbl_colon, ui_config.roller_time_hh, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

#if (CFG_STAGE == 56)
    return;
#endif

    // 57: create MM roller + options
    ui_config.roller_time_mm = lv_roller_create(time_content);
    lv_obj_set_size(ui_config.roller_time_mm, kRollerTimeW, kRollerH);
    lv_roller_set_visible_row_count(ui_config.roller_time_mm, 3);
    style_roller_green(ui_config.roller_time_mm);

    // place MM after colon
    lv_obj_align_to(ui_config.roller_time_mm, lbl_colon, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    static char mm_opts[256];
    mm_opts[0] = '\0';
    for (int m = 0; m <= (60 - UI_MIN_MINUTES); m += UI_MIN_MINUTES) {
        char line[8];
        std::snprintf(line, sizeof(line), "%02d\n", m);
        std::strncat(mm_opts, line, sizeof(mm_opts) - std::strlen(mm_opts) - 1);
    }
    size_t len_mm = std::strlen(mm_opts);
    if (len_mm > 0 && mm_opts[len_mm - 1] == '\n') {
        mm_opts[len_mm - 1] = '\0';
    }
    lv_roller_set_options(ui_config.roller_time_mm, mm_opts, LV_ROLLER_MODE_NORMAL);

#if (CFG_STAGE == 57)
    return;
#endif

    // 58: create TEMP card only
    lv_obj_t *temp_content = nullptr;
    lv_obj_t *card_temp = create_card(zone_b, kCardW_Small, kCardH, "DRYING TEMP", &temp_content);
    if (!temp_content) {
        temp_content = card_temp;
    }

    lv_obj_clear_flag(temp_content, LV_OBJ_FLAG_SCROLLABLE);
    // (TEMP can stay flex-column from create_card; no change needed)

#if (CFG_STAGE == 58)
    return;
#endif

    // 59: create TEMP roller (NO options yet)
    ui_config.roller_drying_temp = lv_roller_create(temp_content);
    lv_obj_set_width(ui_config.roller_drying_temp, kRollerTempW);
    lv_obj_set_height(ui_config.roller_drying_temp, kRollerH);
    lv_roller_set_visible_row_count(ui_config.roller_drying_temp, 3);

#if (CFG_STAGE == 59)
    return;
#endif

    // 60: style + set TEMP options
    style_roller_green(ui_config.roller_drying_temp);

    static char temp_opts[1024];
    temp_opts[0] = '\0';
    for (int t = 0; t <= 120; ++t) {
        char line[12];
        std::snprintf(line, sizeof(line), "%d°\n", t);
        std::strncat(temp_opts, line, sizeof(temp_opts) - std::strlen(temp_opts) - 1);
    }
    size_t len_t = std::strlen(temp_opts);
    if (len_t > 0 && temp_opts[len_t - 1] == '\n') {
        temp_opts[len_t - 1] = '\0';
    }
    lv_roller_set_options(ui_config.roller_drying_temp, temp_opts, LV_ROLLER_MODE_NORMAL);

#if (CFG_STAGE == 60)
    return;
#endif

    lv_obj_add_event_cb(ui_config.roller_time_hh, hh_roller_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(ui_config.roller_time_mm, mm_roller_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(ui_config.roller_drying_temp, temp_roller_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ------------------------------------------------------------
    // Preload selected preset and load values to widgets
    // ------------------------------------------------------------
    int idx = oven_get_current_preset_index();
    if (idx < 0 || idx >= n) {
        idx = 0;
    }

    set_roller_value_silent(ui_config.roller_filament_type, idx);
    load_preset_to_widgets(idx);
}

static void create_buttons(lv_obj_t *parent) {
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

static void create_icons(lv_obj_t *parent) {
    // parent is base.left (the left column from screen_base)
    // We create ONE inner container for spacing, inside that left column.
    lv_obj_t *icon_col = lv_obj_create(parent);
    lv_obj_remove_style_all(icon_col);

    // Fill the left column (or set fixed height if you prefer)
    lv_obj_set_size(icon_col, LV_PCT(100), LV_PCT(100));
    lv_obj_center(icon_col);

    lv_obj_set_style_bg_opa(icon_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(icon_col, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(icon_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(icon_col,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // This is the spacing between icons (same purpose as before)
    lv_obj_set_style_pad_row(icon_col, 16, 0);

    // Keep ui_config.icons_container pointing to the column container
    ui_config.icons_container = icon_col;

    // --- Create icons (same pipeline as screen_main) ---
    ui_config.icon_fan12v = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_fan12v, &fan12v_wht);
    lv_obj_set_size(ui_config.icon_fan12v, 32, 32);

    ui_config.icon_fan230 = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_fan230, &fan230v_fast_wht);
    lv_obj_set_size(ui_config.icon_fan230, 32, 32);
    lv_obj_add_flag(ui_config.icon_fan230, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_fan230, icon_fan230_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_fan230_slow = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_fan230_slow, &fan230v_low_wht);
    lv_obj_set_size(ui_config.icon_fan230_slow, 32, 32);
    lv_obj_add_flag(ui_config.icon_fan230_slow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_fan230_slow, icon_fan230_slow_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_heater = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_heater, &heater_wht);
    lv_obj_set_size(ui_config.icon_heater, 32, 32);
    lv_obj_add_flag(ui_config.icon_heater, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_heater, icon_heater_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_door = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_door, &door_open_wht);
    lv_obj_set_size(ui_config.icon_door, 32, 32);

    ui_config.icon_motor = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_motor, &motor230v);
    lv_obj_set_size(ui_config.icon_motor, 32, 32);
    lv_obj_add_flag(ui_config.icon_motor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_motor, icon_motor_cb, LV_EVENT_CLICKED, nullptr);

    ui_config.icon_lamp = lv_image_create(icon_col);
    lv_image_set_src(ui_config.icon_lamp, &lamp230v_wht);
    lv_obj_set_size(ui_config.icon_lamp, 32, 32);
    lv_obj_add_flag(ui_config.icon_lamp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_config.icon_lamp, icon_lamp_cb, LV_EVENT_CLICKED, nullptr);

    update_icon_enabled();
    update_icon_colors();
}

// -----------------------------------------------------------------------------
// Event-Callback
// -----------------------------------------------------------------------------
static void update_save_enabled(void) {
    if (!ui_config.btn_save) {
        return;
    }

    const bool can_save = !oven_is_running();
    if (can_save) {
        lv_obj_clear_state(ui_config.btn_save, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_config.btn_save, LV_STATE_DISABLED);
    }
}

static void save_state_timer_cb(lv_timer_t *t) {
    (void)t;
    update_save_enabled();
}

static void btn_save_event_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        UI_WARN("[screen_config] SAVE blocked (oven running)\n");
        return;
    }
    // final sync (belt & suspenders)
    apply_runtime_from_widgets();

    UI_INFO("[screen_config] SAVE -> go home\n");
    if (ui_config.label_info_message) {
        lv_label_set_text(ui_config.label_info_message, "Saved (runtime) - returning...");
    }

    screen_manager_go_home();
}

static void btn_clear_event_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
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

    if (ui_config.label_info_message) {
        lv_label_set_text(ui_config.label_info_message, "Cleared (runtime)");
    }

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

static void icons_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    update_icon_enabled();
    update_icon_colors();
}

static void filament_roller_event_cb(lv_event_t *e) {
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }

    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    const int idx = lv_roller_get_selected(roller);

    UI_INFO("[screen_config] preset selected index=%d\n", idx);

    // 1) Set runtime to this preset (id/name/rotary)
    oven_select_preset((uint16_t)idx);

    // 2) Load preset defaults into widgets
    load_preset_to_widgets(idx);

    // Optional: also write the loaded defaults to runtime immediately
    // (safe because widgets now match preset values)
    apply_runtime_from_widgets();
}

static void temp_roller_event_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    apply_runtime_from_widgets();
}

static void hh_roller_event_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    apply_runtime_from_widgets();
}

static void mm_roller_event_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    apply_runtime_from_widgets();
}

static void icon_toggle_event_cb(lv_event_t *e) {
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        UI_WARN("[screen_config] icon toggle blocked (oven running)\n");
        return;
    }

    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    const int id = (int)(intptr_t)lv_event_get_user_data(e);

    switch (id) {
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

static void icon_fan230_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        return;
    }
    s_fan230 = !s_fan230;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_fan230(s_fan230);
}

static void icon_fan230_slow_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        return;
    }
    s_fan230_slow = !s_fan230_slow;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_fan230_slow(s_fan230_slow);
}

static void icon_heater_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        return;
    }
    s_heater = !s_heater;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_heater(s_heater);
}

static void icon_motor_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        return;
    }
    s_motor = !s_motor;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_motor(s_motor);
}

static void icon_lamp_cb(lv_event_t *e) {
    (void)e;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    if (oven_is_running()) {
        return;
    }
    s_lamp = !s_lamp;
    update_icon_colors();
    // TODO next: oven_set_runtime_actuator_lamp(s_lamp);
}

static void icons_state_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!s_screen_ready || s_updating_widgets) {
        return;
    }
    update_icons_enabled();
}
// -----------------------------------------------------------------------------
// HELPERS
// -----------------------------------------------------------------------------
static inline lv_color_t col_hex(uint32_t hex) { return ui_color_from_hex(hex); }

static void apply_runtime_from_widgets(void) {
    if (!ui_config.roller_filament_type ||
        !ui_config.roller_time_hh ||
        !ui_config.roller_time_mm ||
        !ui_config.roller_drying_temp) {
        return;
    }

    // Filament type / preset
    const int preset_idx = lv_roller_get_selected(ui_config.roller_filament_type);
    oven_select_preset((uint16_t)preset_idx); // sets filamentId, presetName, rotaryOn (and maybe defaults)

    // HH:MM
    const int hh = lv_roller_get_selected(ui_config.roller_time_hh);
    const int mm5 = lv_roller_get_selected(ui_config.roller_time_mm);

    // it is possible to adjust the granularity of Minutes. For Test set to 1min, Runtime-System 5min is a good approach
    const int mm = mm5 * UI_MIN_MINUTES;

    const int duration_min = hh * 60 + mm;

    // Temperature roller is 20..120 mapped by index 0..100
    const int temp_idx = lv_roller_get_selected(ui_config.roller_drying_temp);
    const int temp_c = temp_idx;

    // Apply to runtime only (no persistence)
    oven_set_runtime_duration_minutes((uint16_t)duration_min);
    oven_set_runtime_temp_target((uint16_t)temp_c);

    if (ui_config.label_info_message) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Runtime: %dC / %02d:%02d", temp_c, hh, mm);
        lv_label_set_text(ui_config.label_info_message, buf);
    }

    UI_INFO("[screen_config] runtime updated: temp=%dC duration=%dmin\n", temp_c, duration_min);
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
lv_obj_t *screen_config_create(lv_obj_t *parent) {
    UI_INFO("[CFG] create enter parent=%p\n", (void *)parent);
    s_screen_ready = false;
    static lv_timer_t *s_tmr_save = nullptr;
    static lv_timer_t *s_tmr_icons = nullptr;

    // Return existing instance
    if (ui_config.root != nullptr) {
        UI_INFO("[screen_config] reusing existing root\n");
        return ui_config.root;
    }

    // Build base layout (480x480)
    ScreenBaseLayout base{};
    screen_base_create(&base, parent,
                       UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT,
                       BASE_TOP_H, BASE_PAGE_INDICATOR_H, BASE_BOTTOM_H, // top_h, page_h, bottom_h
                       BASE_SIDE_W, BASE_CENTER_W);                      // side_w, center_w

    UI_INFO("[BASE-CFG] (1) base created root=%p top=%p center=%p page=%p bottom=%p left=%p mid=%p right=%p\n",
            (void *)base.root,
            (void *)base.top,
            (void *)base.center,
            (void *)base.page_indicator,
            (void *)base.bottom,
            (void *)base.left, (void *)base.middle, (void *)base.right);

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
    if (!s_tmr_save) {
        s_tmr_save = lv_timer_create(save_state_timer_cb, 250, NULL);
    }

    update_save_enabled();

    lv_obj_update_layout(base.root);
    UI_INFO("[BASE-CFG] (2) SIZE: root=%d top=%d center=%d page=%d bottom=%d\n",
            lv_obj_get_height(base.root),
            lv_obj_get_height(base.top),
            lv_obj_get_height(base.center),
            lv_obj_get_height(base.page_indicator),
            lv_obj_get_height(base.bottom));

    create_icons(ui_config.icons_container);
    // lv_timer_create(icons_state_timer_cb, 250, NULL);
    if (!s_tmr_icons) {
        s_tmr_icons = lv_timer_create(icons_state_timer_cb, 250, NULL);
    }
    update_icons_enabled();
    update_icon_colors();

    // ----------- PLACEHOLDERS
    // Simple placeholders so the screen is clearly visible
    create_top_placeholder(ui_config.top_bar_container);
    create_bottom_placeholder(ui_config.bottom_container);

    UI_INFO("[screen_config] created root=%p swipe_target=%p\n",
            (void *)ui_config.root, (void *)ui_config.s_swipe_target);
    s_screen_ready = true;
    return ui_config.root;
}

lv_obj_t *screen_config_get_swipe_target(void) {
    return ui_config.s_swipe_target;
}

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static void create_page_indicator(lv_obj_t *parent) {
    // Make the whole page-indicator container the swipe target
    ui_config.s_swipe_target = parent;

    // Must be clickable to receive pointer events
    lv_obj_add_flag(ui_config.s_swipe_target, LV_OBJ_FLAG_CLICKABLE);

    // Very subtle hint so user knows where to swipe
    // (you can tune LV_OPA_4..LV_OPA_10)
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_35, 0);
    lv_obj_set_style_radius(parent, 10, 0);

    // Inner panel (rounded rectangle) + dots (optional visual consistency)
    ui_config.page_indicator_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_config.page_indicator_panel);
    lv_obj_set_size(ui_config.page_indicator_panel, 100, 24);
    lv_obj_center(ui_config.page_indicator_panel);

    lv_obj_set_style_radius(ui_config.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui_config.page_indicator_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(ui_config.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_config.page_indicator_panel, 2, 0);

    lv_obj_set_flex_flow(ui_config.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_config.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        ui_config.page_dots[i] = lv_obj_create(ui_config.page_indicator_panel);
        lv_obj_remove_style_all(ui_config.page_dots[i]);
        lv_obj_set_size(ui_config.page_dots[i], 10, 10);
        lv_obj_set_style_radius(ui_config.page_dots[i], 5, 0);
        lv_obj_set_style_bg_opa(ui_config.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui_config.page_dots[i],
                                  (i == 1) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x606060), 0);
        if (i > 0) {
            lv_obj_set_style_margin_left(ui_config.page_dots[i], 6, 0);
        }
    }
}

static void create_top_placeholder(lv_obj_t *parent) {
    // Just a title so we see we're on CONFIG
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "CONFIG");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

static void create_bottom_placeholder(lv_obj_t *parent) {
    // Bottom info message placeholder
    ui_config.label_info_message = lv_label_create(parent);
    lv_label_set_text(ui_config.label_info_message, "Swipe here to change screens");
    lv_obj_set_style_text_color(ui_config.label_info_message, lv_color_hex(0xB0B0B0), 0);
    lv_obj_center(ui_config.label_info_message);
}

void screen_config_set_active_page(uint8_t page_index) {
    if (page_index >= UI_PAGE_COUNT) {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        lv_color_t col = (i == page_index)
                             ? col_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : col_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui_config.page_dots[i], col, 0);
    }
}