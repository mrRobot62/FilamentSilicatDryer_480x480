#include "screen_parameters.h"

#include "host_parameters.h"
#include "ui_color_codes.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

static constexpr lv_coord_t kBaseTopH = 48;
static constexpr lv_coord_t kBasePageH = 40;
static constexpr lv_coord_t kBaseBottomH = 72;
static constexpr lv_coord_t kBaseSideW = 60;
static constexpr lv_coord_t kBaseCenterW = 360;

static constexpr uint32_t kColorCardBg = 0x1E1E1E;
static constexpr uint32_t kColorCardBorder = 0x4A4A4A;
static constexpr uint32_t kColorAccent = 0x00A000;
static constexpr uint32_t kColorSave = 0x2D7A32;
static constexpr uint32_t kColorSaveDisabled = 0x444444;
static constexpr uint32_t kColorReset = 0xA82020;
static constexpr uint32_t kColorSubtle = 0xA8A8A8;
static constexpr uint32_t kColorStepperBg = 0x2A2A2A;
static constexpr uint32_t kColorStepperBorder = 0x5E5E5E;

enum HeaterField : uint8_t {
    HEATER_FIELD_TARGET = 0,
    HEATER_FIELD_HYSTERESIS,
    HEATER_FIELD_APPROACH,
    HEATER_FIELD_HOLD,
    HEATER_FIELD_OVERSHOOT
};

struct HeaterProfileUiState {
    int16_t targetC;
    int16_t hysteresis_dC;
    int16_t approachBand_dC;
    int16_t holdBand_dC;
    int16_t overshootCap_dC;
};

static HostParameters s_saved_parameters = {};
static bool s_internal_update = false;

static inline lv_color_t col_hex(uint32_t hex) { return ui_color_from_hex(hex); }

static void create_page_indicator(lv_obj_t *parent);
static void create_top_bar(lv_obj_t *parent);
static void create_scroll_content(lv_obj_t *parent);
static void create_bottom_actions(lv_obj_t *parent);

static lv_obj_t *create_group_card(lv_obj_t *parent, const char *title);
static void create_shortcuts_group(lv_obj_t *parent);
static void create_heater_group(lv_obj_t *parent);
static lv_obj_t *create_stepper(lv_obj_t *parent, lv_obj_t **out_spinbox,
                                int32_t min_value, int32_t max_value, int32_t step,
                                int32_t default_value, bool one_decimal, lv_coord_t width);
static void create_profile_card(lv_obj_t *parent, const char *title, uint8_t profile_index);

static void button_reset_event_cb(lv_event_t *e);
static void button_save_event_cb(lv_event_t *e);
static void spinbox_increment_event_cb(lv_event_t *e);
static void spinbox_decrement_event_cb(lv_event_t *e);
static void spinbox_value_changed_cb(lv_event_t *e);

static void reset_widgets_to_defaults(void);
static void load_saved_state_into_widgets(void);
static void read_widgets_into_parameters(HostParameters *out);
static bool screen_has_unsaved_changes(void);
static void update_shortcut_button_labels(void);
static void update_save_button_state(void);
static void set_info_message(const char *text, uint32_t color_hex);
static void abbreviate_preset_name(uint16_t preset_id, char *out, size_t out_len);
static int16_t read_spinbox_value(lv_obj_t *spinbox);
static bool heater_profile_equals(const HeaterProfileUiState &lhs, const HeaterProfileUiState &rhs);

static lv_obj_t *create_group_card(lv_obj_t *parent, const char *title) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, col_hex(kColorCardBorder), 0);
    lv_obj_set_style_bg_color(card, col_hex(kColorCardBg), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_90, 0);

    return card;
}

static void style_small_button(lv_obj_t *btn, uint32_t bg_hex, lv_coord_t radius) {
    lv_obj_set_style_radius(btn, radius, 0);
    lv_obj_set_style_bg_color(btn, col_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, col_hex(kColorStepperBorder), 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_80, 0);
}

static lv_obj_t *create_stepper(lv_obj_t *parent, lv_obj_t **out_spinbox,
                                int32_t min_value, int32_t max_value, int32_t step,
                                int32_t default_value, bool one_decimal, lv_coord_t width) {
    const lv_coord_t button_w = (width <= 80) ? 18 : 24;
    const lv_coord_t button_h = (width <= 80) ? 22 : 26;
    const lv_coord_t spinbox_w = width - (2 * button_w) - 8;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, width, button_h + 4);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_minus = lv_btn_create(row);
    lv_obj_set_size(btn_minus, button_w, button_h);
    style_small_button(btn_minus, kColorStepperBg, 6);
    lv_obj_add_event_cb(btn_minus, spinbox_decrement_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "-");
    lv_obj_center(lbl_minus);

    lv_obj_t *spinbox = lv_spinbox_create(row);
    lv_obj_set_size(spinbox, spinbox_w, button_h + 2);
    lv_spinbox_set_range(spinbox, min_value, max_value);
    lv_spinbox_set_step(spinbox, step);
    lv_spinbox_set_value(spinbox, default_value);
    if (one_decimal) {
        lv_spinbox_set_digit_format(spinbox, 3, 1);
    } else {
        lv_spinbox_set_digit_format(spinbox, 3, 0);
    }
    lv_obj_set_style_bg_color(spinbox, col_hex(kColorStepperBg), 0);
    lv_obj_set_style_bg_opa(spinbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(spinbox, 1, 0);
    lv_obj_set_style_border_color(spinbox, col_hex(kColorStepperBorder), 0);
    lv_obj_set_style_radius(spinbox, 6, 0);
    lv_obj_set_style_text_color(spinbox, lv_color_white(), 0);
    lv_obj_set_style_text_align(spinbox, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(spinbox, spinbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *btn_plus = lv_btn_create(row);
    lv_obj_set_size(btn_plus, button_w, button_h);
    style_small_button(btn_plus, kColorStepperBg, 6);
    lv_obj_add_event_cb(btn_plus, spinbox_increment_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "+");
    lv_obj_center(lbl_plus);

    lv_obj_set_user_data(btn_minus, spinbox);
    lv_obj_set_user_data(btn_plus, spinbox);

    if (out_spinbox) {
        *out_spinbox = spinbox;
    }
    return row;
}

static void create_shortcuts_group(lv_obj_t *parent) {
    HostParameters defaults{};
    host_parameters_get_defaults(&defaults);

    ui_parameters.group_shortcuts = create_group_card(parent, "Filament-ShortCuts");

    lv_obj_t *row = lv_obj_create(ui_parameters.group_shortcuts);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_obj_t *slot = lv_obj_create(row);
        lv_obj_remove_style_all(slot);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(slot, 78, 118);
        lv_obj_set_style_pad_row(slot, 8, 0);
        lv_obj_set_flex_flow(slot, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *slot_label = lv_label_create(slot);
        char slot_text[8];
        std::snprintf(slot_text, sizeof(slot_text), "F%u", (unsigned)(i + 1));
        lv_label_set_text(slot_label, slot_text);
        lv_obj_set_style_text_color(slot_label, col_hex(kColorSubtle), 0);

        ui_parameters.shortcut_button[i] = lv_btn_create(slot);
        lv_obj_set_size(ui_parameters.shortcut_button[i], 70, 42);
        style_small_button(ui_parameters.shortcut_button[i], 0x303030, 10);
        lv_obj_clear_flag(ui_parameters.shortcut_button[i], LV_OBJ_FLAG_CLICKABLE);

        ui_parameters.shortcut_button_label[i] = lv_label_create(ui_parameters.shortcut_button[i]);
        lv_label_set_text(ui_parameters.shortcut_button_label[i], "---");
        lv_obj_set_style_text_color(ui_parameters.shortcut_button_label[i], lv_color_white(), 0);
        lv_obj_center(ui_parameters.shortcut_button_label[i]);

        create_stepper(slot, &ui_parameters.shortcut_spinbox[i], 0, kPresetCount - 1, 1,
                       defaults.shortcutPresetIds[i], false, 78);
        lv_spinbox_set_digit_format(ui_parameters.shortcut_spinbox[i], 2, 0);
    }
}

static void create_profile_row(lv_obj_t *parent, const char *label_text, lv_obj_t **out_spinbox,
                               int32_t min_value, int32_t max_value, int32_t step, int32_t default_value,
                               bool one_decimal) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, col_hex(kColorSubtle), 0);

    create_stepper(row, out_spinbox, min_value, max_value, step, default_value, one_decimal, 96);
}

static void create_profile_card(lv_obj_t *parent, const char *title, uint8_t profile_index) {
    HostParameters defaults{};
    host_parameters_get_defaults(&defaults);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, 158, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, col_hex(kColorCardBorder), 0);
    lv_obj_set_style_bg_color(card, col_hex(0x262626), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, col_hex(kColorAccent), 0);

    const HostHeaterProfileParameters &defaults_profile = defaults.heaterProfiles[profile_index];
    create_profile_row(card, "TGT", &ui_parameters.heater_spinbox[profile_index][HEATER_FIELD_TARGET],
                       30, 120, 1, defaults_profile.targetC, false);
    create_profile_row(card, "HYS", &ui_parameters.heater_spinbox[profile_index][HEATER_FIELD_HYSTERESIS],
                       5, 50, 5, defaults_profile.hysteresis_dC, true);
    create_profile_row(card, "APR", &ui_parameters.heater_spinbox[profile_index][HEATER_FIELD_APPROACH],
                       10, 200, 5, defaults_profile.approachBand_dC, true);
    create_profile_row(card, "HLD", &ui_parameters.heater_spinbox[profile_index][HEATER_FIELD_HOLD],
                       5, 100, 5, defaults_profile.holdBand_dC, true);
    create_profile_row(card, "OVR", &ui_parameters.heater_spinbox[profile_index][HEATER_FIELD_OVERSHOOT],
                       5, 50, 5, defaults_profile.overshootCap_dC, true);
}

static void create_heater_group(lv_obj_t *parent) {
    ui_parameters.group_heater = create_group_card(parent, "Heater-Curve");

    lv_obj_t *hint = lv_label_create(ui_parameters.group_heater);
    lv_label_set_text(hint, "Preset-Kennwerte fuer 45/60/80/100");
    lv_obj_set_style_text_color(hint, col_hex(kColorSubtle), 0);

    lv_obj_t *grid = lv_obj_create(ui_parameters.group_heater);
    lv_obj_remove_style_all(grid);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    create_profile_card(grid, "45C", 0);
    create_profile_card(grid, "60C", 1);
    create_profile_card(grid, "80C", 2);
    create_profile_card(grid, "100C", 3);
}

static void create_top_bar(lv_obj_t *parent) {
    ui_parameters.label_title = lv_label_create(parent);
    lv_label_set_text(ui_parameters.label_title, "Parameter");
    lv_obj_set_style_text_color(ui_parameters.label_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_parameters.label_title, LV_FONT_DEFAULT, 0);
    lv_obj_align(ui_parameters.label_title, LV_ALIGN_CENTER, 0, 0);
}

static void create_scroll_content(lv_obj_t *parent) {
    ui_parameters.content_scroll = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_parameters.content_scroll);
    lv_obj_set_size(ui_parameters.content_scroll, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scroll_dir(ui_parameters.content_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_parameters.content_scroll, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_all(ui_parameters.content_scroll, 4, 0);
    lv_obj_set_style_pad_row(ui_parameters.content_scroll, 10, 0);
    lv_obj_set_flex_flow(ui_parameters.content_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_parameters.content_scroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    create_shortcuts_group(ui_parameters.content_scroll);
    create_heater_group(ui_parameters.content_scroll);
}

static void create_page_indicator(lv_obj_t *parent) {
    ui_parameters.s_swipe_target = parent;
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_10, 0);
    lv_obj_set_style_radius(parent, 10, 0);

    ui_parameters.page_indicator_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_parameters.page_indicator_panel);
    lv_obj_set_size(ui_parameters.page_indicator_panel, 100, 24);
    lv_obj_center(ui_parameters.page_indicator_panel);
    lv_obj_set_style_radius(ui_parameters.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui_parameters.page_indicator_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(ui_parameters.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_parameters.page_indicator_panel, 4, 0);
    lv_obj_set_flex_flow(ui_parameters.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_parameters.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        ui_parameters.page_dots[i] = lv_obj_create(ui_parameters.page_indicator_panel);
        lv_obj_remove_style_all(ui_parameters.page_dots[i]);
        lv_obj_set_size(ui_parameters.page_dots[i], 10, 10);
        lv_obj_set_style_radius(ui_parameters.page_dots[i], 5, 0);
        lv_obj_set_style_bg_opa(ui_parameters.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui_parameters.page_dots[i],
                                  (i == SCREEN_PARAMETERS) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x606060), 0);
        if (i > 0) {
            lv_obj_set_style_margin_left(ui_parameters.page_dots[i], 8, 0);
        }
    }
}

static void create_bottom_actions(lv_obj_t *parent) {
    lv_obj_set_style_pad_hor(parent, 16, 0);
    lv_obj_set_style_pad_ver(parent, 10, 0);

    ui_parameters.btn_reset = lv_btn_create(parent);
    lv_obj_set_size(ui_parameters.btn_reset, 120, 46);
    lv_obj_align(ui_parameters.btn_reset, LV_ALIGN_LEFT_MID, 12, 0);
    style_small_button(ui_parameters.btn_reset, kColorReset, 12);
    lv_obj_add_event_cb(ui_parameters.btn_reset, button_reset_event_cb, LV_EVENT_CLICKED, nullptr);

    ui_parameters.label_btn_reset = lv_label_create(ui_parameters.btn_reset);
    lv_label_set_text(ui_parameters.label_btn_reset, "RESET");
    lv_obj_set_style_text_color(ui_parameters.label_btn_reset, lv_color_white(), 0);
    lv_obj_center(ui_parameters.label_btn_reset);

    ui_parameters.btn_save = lv_btn_create(parent);
    lv_obj_set_size(ui_parameters.btn_save, 120, 46);
    lv_obj_align(ui_parameters.btn_save, LV_ALIGN_RIGHT_MID, -12, 0);
    style_small_button(ui_parameters.btn_save, kColorSave, 12);
    lv_obj_add_event_cb(ui_parameters.btn_save, button_save_event_cb, LV_EVENT_CLICKED, nullptr);

    ui_parameters.label_btn_save = lv_label_create(ui_parameters.btn_save);
    lv_label_set_text(ui_parameters.label_btn_save, "SAVE");
    lv_obj_set_style_text_color(ui_parameters.label_btn_save, lv_color_white(), 0);
    lv_obj_center(ui_parameters.label_btn_save);

    ui_parameters.label_info_message = lv_label_create(parent);
    lv_label_set_text(ui_parameters.label_info_message, "HOST Parameter lokal anpassen");
    lv_obj_set_width(ui_parameters.label_info_message, 180);
    lv_obj_set_style_text_align(ui_parameters.label_info_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui_parameters.label_info_message, col_hex(kColorSubtle), 0);
    lv_obj_align(ui_parameters.label_info_message, LV_ALIGN_CENTER, 0, 0);
}

static void set_info_message(const char *text, uint32_t color_hex) {
    if (!ui_parameters.label_info_message) {
        return;
    }
    lv_label_set_text(ui_parameters.label_info_message, text);
    lv_obj_set_style_text_color(ui_parameters.label_info_message, col_hex(color_hex), 0);
}

static int16_t read_spinbox_value(lv_obj_t *spinbox) {
    return spinbox ? static_cast<int16_t>(lv_spinbox_get_value(spinbox)) : 0;
}

static void abbreviate_preset_name(uint16_t preset_id, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return;
    }

    const FilamentPreset *preset = oven_get_preset(preset_id);
    if (!preset || !preset->name) {
        std::snprintf(out, out_len, "%u", (unsigned)preset_id);
        return;
    }

    const char *name = preset->name;
    if (std::strncmp(name, "Spec-", 5) == 0) {
        name += 5;
    }

    char compact[16] = {0};
    size_t compact_len = 0;
    for (size_t i = 0; name[i] != '\0' && compact_len < sizeof(compact) - 1; ++i) {
        char c = name[i];
        if ((c >= 'a') && (c <= 'z')) {
            c = static_cast<char>(c - ('a' - 'A'));
        }
        const bool is_alpha = (c >= 'A' && c <= 'Z');
        if (!is_alpha) {
            if (compact_len > 0) {
                break;
            }
            continue;
        }
        compact[compact_len++] = c;
    }

    if (compact_len == 0) {
        std::snprintf(out, out_len, "%u", (unsigned)preset_id);
        return;
    }

    compact[(compact_len > 5) ? 5 : compact_len] = '\0';
    std::snprintf(out, out_len, "%s", compact);
}

static void update_shortcut_button_labels(void) {
    char text[8];
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        abbreviate_preset_name(static_cast<uint16_t>(read_spinbox_value(ui_parameters.shortcut_spinbox[i])),
                               text, sizeof(text));
        lv_label_set_text(ui_parameters.shortcut_button_label[i], text);
    }
}

static bool heater_profile_equals(const HeaterProfileUiState &lhs, const HeaterProfileUiState &rhs) {
    return lhs.targetC == rhs.targetC &&
           lhs.hysteresis_dC == rhs.hysteresis_dC &&
           lhs.approachBand_dC == rhs.approachBand_dC &&
           lhs.holdBand_dC == rhs.holdBand_dC &&
           lhs.overshootCap_dC == rhs.overshootCap_dC;
}

static bool screen_has_unsaved_changes(void) {
    HostParameters current{};
    read_widgets_into_parameters(&current);

    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        if (current.shortcutPresetIds[i] != s_saved_parameters.shortcutPresetIds[i]) {
            return true;
        }
    }

    for (uint8_t i = 0; i < UI_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        HeaterProfileUiState current_profile = {
            current.heaterProfiles[i].targetC,
            current.heaterProfiles[i].hysteresis_dC,
            current.heaterProfiles[i].approachBand_dC,
            current.heaterProfiles[i].holdBand_dC,
            current.heaterProfiles[i].overshootCap_dC,
        };
        HeaterProfileUiState saved_profile = {
            s_saved_parameters.heaterProfiles[i].targetC,
            s_saved_parameters.heaterProfiles[i].hysteresis_dC,
            s_saved_parameters.heaterProfiles[i].approachBand_dC,
            s_saved_parameters.heaterProfiles[i].holdBand_dC,
            s_saved_parameters.heaterProfiles[i].overshootCap_dC,
        };
        if (!heater_profile_equals(current_profile, saved_profile)) {
            return true;
        }
    }
    return false;
}

static void update_save_button_state(void) {
    const bool dirty = screen_has_unsaved_changes();
    const uint32_t bg = dirty ? kColorSave : kColorSaveDisabled;
    lv_obj_set_style_bg_color(ui_parameters.btn_save, col_hex(bg), 0);
    if (dirty) {
        lv_obj_clear_state(ui_parameters.btn_save, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_parameters.btn_save, LV_STATE_DISABLED);
    }
}

static void reset_widgets_to_defaults(void) {
    HostParameters defaults{};
    host_parameters_get_defaults(&defaults);

    s_internal_update = true;
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_spinbox_set_value(ui_parameters.shortcut_spinbox[i], defaults.shortcutPresetIds[i]);
    }
    for (uint8_t i = 0; i < UI_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        const HostHeaterProfileParameters &profile = defaults.heaterProfiles[i];
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_TARGET], profile.targetC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HYSTERESIS], profile.hysteresis_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_APPROACH], profile.approachBand_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HOLD], profile.holdBand_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_OVERSHOOT], profile.overshootCap_dC);
    }
    s_internal_update = false;
    update_shortcut_button_labels();
    update_save_button_state();
}

static void load_saved_state_into_widgets(void) {
    s_internal_update = true;
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_spinbox_set_value(ui_parameters.shortcut_spinbox[i], s_saved_parameters.shortcutPresetIds[i]);
    }
    for (uint8_t i = 0; i < UI_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_TARGET], s_saved_parameters.heaterProfiles[i].targetC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HYSTERESIS], s_saved_parameters.heaterProfiles[i].hysteresis_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_APPROACH], s_saved_parameters.heaterProfiles[i].approachBand_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HOLD], s_saved_parameters.heaterProfiles[i].holdBand_dC);
        lv_spinbox_set_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_OVERSHOOT], s_saved_parameters.heaterProfiles[i].overshootCap_dC);
    }
    s_internal_update = false;
    update_shortcut_button_labels();
    update_save_button_state();
}

static void read_widgets_into_parameters(HostParameters *out) {
    if (!out) {
        return;
    }

    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        out->shortcutPresetIds[i] = static_cast<uint16_t>(read_spinbox_value(ui_parameters.shortcut_spinbox[i]));
    }
    for (uint8_t i = 0; i < UI_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        out->heaterProfiles[i] = {
            read_spinbox_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_TARGET]),
            read_spinbox_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HYSTERESIS]),
            read_spinbox_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_APPROACH]),
            read_spinbox_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_HOLD]),
            read_spinbox_value(ui_parameters.heater_spinbox[i][HEATER_FIELD_OVERSHOOT]),
        };
    }
}

static void spinbox_increment_event_cb(lv_event_t *e) {
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_obj_t *spinbox = static_cast<lv_obj_t *>(lv_obj_get_user_data(target));
    if (!spinbox) {
        return;
    }
    lv_spinbox_increment(spinbox);
    spinbox_value_changed_cb(e);
}

static void spinbox_decrement_event_cb(lv_event_t *e) {
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_obj_t *spinbox = static_cast<lv_obj_t *>(lv_obj_get_user_data(target));
    if (!spinbox) {
        return;
    }
    lv_spinbox_decrement(spinbox);
    spinbox_value_changed_cb(e);
}

static void spinbox_value_changed_cb(lv_event_t *e) {
    LV_UNUSED(e);
    update_shortcut_button_labels();
    update_save_button_state();
    if (!s_internal_update) {
        set_info_message("Nicht gespeicherte Aenderungen", 0xF0B040);
    }
}

static void button_reset_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    reset_widgets_to_defaults();
    set_info_message("Default-Werte geladen", 0xFF7070);
}

static void button_save_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    HostParameters candidate{};
    read_widgets_into_parameters(&candidate);
    if (!host_parameters_save(&candidate)) {
        set_info_message("SAVE fehlgeschlagen", 0xFF7070);
        return;
    }

    s_saved_parameters = candidate;
    update_save_button_state();
    set_info_message("In NVM gespeichert", 0x70D070);
}

} // namespace

lv_obj_t *screen_parameters_create(lv_obj_t *parent) {
    if (ui_parameters.root != nullptr) {
        return ui_parameters.root;
    }

    ScreenBaseLayout base{};
    screen_base_create(&base, parent,
                       UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT,
                       kBaseTopH, kBasePageH, kBaseBottomH,
                       kBaseSideW, kBaseCenterW);

    ui_parameters.root = base.root;
    ui_parameters.top_bar_container = base.top;
    ui_parameters.center_container = base.center;
    ui_parameters.page_indicator_container = base.page_indicator;
    ui_parameters.bottom_container = base.bottom;
    ui_parameters.icons_container = base.left;
    ui_parameters.config_container = base.middle;
    ui_parameters.button_container = base.right;

    create_top_bar(ui_parameters.top_bar_container);
    create_scroll_content(ui_parameters.config_container);
    create_page_indicator(ui_parameters.page_indicator_container);
    create_bottom_actions(ui_parameters.bottom_container);

    host_parameters_get(&s_saved_parameters);
    load_saved_state_into_widgets();
    set_info_message("HOST Parameter lokal anpassen", kColorSubtle);

    return ui_parameters.root;
}

lv_obj_t *screen_parameters_get_swipe_target(void) {
    return ui_parameters.s_swipe_target;
}

void screen_parameters_set_active_page(uint8_t page_index) {
    if (page_index >= UI_PAGE_COUNT) {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        const lv_color_t col = (i == page_index)
                                   ? col_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                                   : col_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui_parameters.page_dots[i], col, 0);
    }
}
