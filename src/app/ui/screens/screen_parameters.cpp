#include "screen_parameters.h"

#include <Arduino.h>

#include "host_parameters.h"
#include "ui_color_codes.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

static constexpr lv_coord_t kBaseTopH = 48;
static constexpr lv_coord_t kBasePageH = 40;
static constexpr lv_coord_t kBaseBottomH = 72;
static constexpr lv_coord_t kBaseSideW = 0;
static constexpr lv_coord_t kBaseCenterW = 470;
static constexpr lv_coord_t kShortcutSlotW = 108;
static constexpr lv_coord_t kShortcutButtonW = 96;
static constexpr lv_coord_t kShortcutButtonH = 52;
static constexpr lv_coord_t kShortcutRollerW = 88;
static constexpr lv_coord_t kShortcutRollerH = 34;
static constexpr lv_coord_t kHeaterFieldW = 100;
static constexpr lv_coord_t kStepperButtonW = 32;
static constexpr lv_coord_t kStepperButtonH = 36;
static constexpr lv_coord_t kStepperValueW = 76;
static constexpr lv_coord_t kStepperWidth = (2 * kStepperButtonW) + kStepperValueW + 8;

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

enum ConfirmAction : uint8_t {
    CONFIRM_ACTION_NONE = 0,
    CONFIRM_ACTION_SAVE,
    CONFIRM_ACTION_RESET
};

struct HeaterProfileUiState {
    int16_t targetC;
    int16_t hysteresis_dC;
    int16_t approachBand_dC;
    int16_t holdBand_dC;
    int16_t overshootCap_dC;
};

static HostParameters s_saved_parameters = {};
static HostParameters s_edit_parameters = {};
static bool s_internal_update = false;
static uint8_t s_selected_heater_profile = 0;
static ConfirmAction s_confirm_action = CONFIRM_ACTION_NONE;

static inline lv_color_t col_hex(uint32_t hex) { return ui_color_from_hex(hex); }

static void create_page_indicator(lv_obj_t *parent);
static void create_top_bar(lv_obj_t *parent);
static void create_scroll_content(lv_obj_t *parent);
static void create_bottom_actions(lv_obj_t *parent);

static lv_obj_t *create_group_card(lv_obj_t *parent, const char *title);
static void create_shortcuts_group(lv_obj_t *parent);
static void create_heater_group(lv_obj_t *parent);
static lv_obj_t *create_stepper(lv_obj_t *parent, lv_obj_t **out_value_label,
                                HeaterField field, lv_coord_t width);
static lv_obj_t *create_shortcut_roller(lv_obj_t *parent, uint16_t default_value);
static lv_obj_t *create_heater_profile_roller(lv_obj_t *parent);
static void create_heater_field_row(lv_obj_t *parent, const char *label_text, const char *hint_text,
                                    HeaterField field, lv_coord_t width);
static void create_confirm_overlay(lv_obj_t *parent);

static void button_reset_event_cb(lv_event_t *e);
static void button_save_event_cb(lv_event_t *e);
static void confirm_save_event_cb(lv_event_t *e);
static void confirm_cancel_event_cb(lv_event_t *e);
static void heater_profile_roller_event_cb(lv_event_t *e);
static void spinbox_increment_event_cb(lv_event_t *e);
static void spinbox_decrement_event_cb(lv_event_t *e);
static void spinbox_value_changed_cb(lv_event_t *e);

static void reset_widgets_to_defaults(void);
static void load_saved_state_into_widgets(void);
static void sync_shortcut_widgets_to_edit_parameters(void);
static void sync_visible_heater_widgets_to_edit_parameters(void);
static void load_selected_heater_profile_into_widgets(void);
static bool screen_has_unsaved_changes(void);
static void update_save_button_state(void);
static void set_info_message(const char *text, uint32_t color_hex);
static uint16_t read_shortcut_value(lv_obj_t *roller);
static bool heater_profile_equals(const HeaterProfileUiState &lhs, const HeaterProfileUiState &rhs);
static int16_t get_heater_field_value(const HostHeaterProfileParameters &profile, HeaterField field);
static void set_heater_field_value(HostHeaterProfileParameters &profile, HeaterField field, int16_t value);
static void format_heater_field_value(HeaterField field, int16_t value, char *out, size_t out_size);
static void update_heater_field_display(HeaterField field);
static int16_t clamp_heater_field_value(HeaterField field, int16_t value);
static int16_t heater_field_step(HeaterField field);
static void hide_confirm_overlay(void);
static void show_confirm_overlay(ConfirmAction action);
static void save_parameters_and_reboot(const HostParameters &params);

static lv_obj_t *create_group_card(lv_obj_t *parent, const char *title) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
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

static lv_obj_t *create_stepper(lv_obj_t *parent, lv_obj_t **out_value_label,
                                HeaterField field, lv_coord_t width) {
    const lv_coord_t button_w = kStepperButtonW;
    const lv_coord_t button_h = kStepperButtonH;
    const lv_coord_t value_w = width - (2 * button_w) - 8;

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
    lv_obj_add_event_cb(btn_minus, spinbox_decrement_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "-");
    lv_obj_center(lbl_minus);

    lv_obj_t *value_box = lv_obj_create(row);
    lv_obj_remove_style_all(value_box);
    lv_obj_clear_flag(value_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(value_box, value_w, button_h + 2);
    lv_obj_set_style_bg_color(value_box, col_hex(kColorStepperBg), 0);
    lv_obj_set_style_bg_opa(value_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(value_box, 1, 0);
    lv_obj_set_style_border_color(value_box, col_hex(kColorStepperBorder), 0);
    lv_obj_set_style_radius(value_box, 6, 0);

    lv_obj_t *value_label = lv_label_create(value_box);
    lv_label_set_text(value_label, "0");
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(value_label);

    lv_obj_t *btn_plus = lv_btn_create(row);
    lv_obj_set_size(btn_plus, button_w, button_h);
    style_small_button(btn_plus, kColorStepperBg, 6);
    lv_obj_add_event_cb(btn_plus, spinbox_increment_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(btn_plus, spinbox_increment_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "+");
    lv_obj_center(lbl_plus);

    lv_obj_set_user_data(btn_minus, reinterpret_cast<void *>(static_cast<uintptr_t>(field)));
    lv_obj_set_user_data(btn_plus, reinterpret_cast<void *>(static_cast<uintptr_t>(field | 0x80)));

    if (out_value_label) {
        *out_value_label = value_label;
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
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_obj_t *slot = lv_obj_create(row);
        lv_obj_remove_style_all(slot);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(slot, kShortcutSlotW, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_row(slot, 4, 0);
        lv_obj_set_flex_flow(slot, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *slot_label = lv_label_create(slot);
        char slot_text[8];
        std::snprintf(slot_text, sizeof(slot_text), "F%u", (unsigned)(i + 1));
        lv_label_set_text(slot_label, slot_text);
        lv_obj_set_style_text_color(slot_label, col_hex(kColorSubtle), 0);

        ui_parameters.shortcut_button[i] = lv_btn_create(slot);
        lv_obj_set_size(ui_parameters.shortcut_button[i], kShortcutButtonW, kShortcutButtonH);
        style_small_button(ui_parameters.shortcut_button[i], 0x303030, 10);
        lv_obj_clear_flag(ui_parameters.shortcut_button[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_pad_all(ui_parameters.shortcut_button[i], 0, 0);

        ui_parameters.shortcut_roller[i] = create_shortcut_roller(ui_parameters.shortcut_button[i], defaults.shortcutPresetIds[i]);
    }
}

static lv_obj_t *create_shortcut_roller(lv_obj_t *parent, uint16_t default_value) {
    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_set_size(roller, kShortcutRollerW, kShortcutRollerH);
    lv_obj_center(roller);
    lv_roller_set_visible_row_count(roller, 1);
    lv_obj_set_style_bg_color(roller, col_hex(kColorStepperBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, col_hex(kColorStepperBorder), LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, col_hex(kColorAccent), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_pad_ver(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(roller, 0, LV_PART_MAIN);

    static char opts[2048];
    opts[0] = '\0';
    for (uint16_t i = 0; i < kPresetCount; ++i) {
        const FilamentPreset *preset = oven_get_preset(i);
        if (!preset || !preset->name) {
            continue;
        }
        char line[64];
        std::snprintf(line, sizeof(line), "%s\n", preset->name);
        std::strncat(opts, line, sizeof(opts) - std::strlen(opts) - 1);
    }
    const size_t len = std::strlen(opts);
    if (len > 0 && opts[len - 1] == '\n') {
        opts[len - 1] = '\0';
    }
    lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, default_value, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller, spinbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return roller;
}

static void create_heater_field_row(lv_obj_t *parent, const char *label_text, const char *hint_text,
                                    HeaterField field, lv_coord_t width) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, width);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *head = lv_obj_create(row);
    lv_obj_remove_style_all(head);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(head, 6, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(head);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, col_hex(kColorSubtle), 0);

    create_stepper(head, &ui_parameters.heater_value_label[field], field, kStepperWidth);

    lv_obj_t *hint = lv_label_create(row);
    lv_label_set_text(hint, hint_text);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_text_color(hint, col_hex(0x8C8C8C), 0);
    lv_obj_set_style_text_opa(hint, LV_OPA_80, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_letter_space(hint, 1, 0);
}

static lv_obj_t *create_heater_profile_roller(lv_obj_t *parent) {
    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_set_width(roller, LV_PCT(100));
    lv_obj_set_height(roller, 36);
    lv_roller_set_visible_row_count(roller, 1);
    lv_obj_set_style_bg_color(roller, col_hex(kColorStepperBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, col_hex(kColorStepperBorder), LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, col_hex(kColorAccent), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    lv_roller_set_options(roller, "45C\n60C\n80C\n100C", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, s_selected_heater_profile, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller, heater_profile_roller_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return roller;
}

static void create_heater_group(lv_obj_t *parent) {
    ui_parameters.group_heater = create_group_card(parent, "Heater-Curve");

    lv_obj_t *hint = lv_label_create(ui_parameters.group_heater);
    lv_label_set_text(hint, "Preset-Kennwerte fuer 45/60/80/100");
    lv_obj_set_style_text_color(hint, col_hex(kColorSubtle), 0);

    lv_obj_t *preset_row = lv_obj_create(ui_parameters.group_heater);
    lv_obj_remove_style_all(preset_row);
    lv_obj_clear_flag(preset_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(preset_row, LV_PCT(100));
    lv_obj_set_height(preset_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(preset_row, 8, 0);
    lv_obj_set_style_pad_right(preset_row, 8, 0);
    lv_obj_set_style_pad_row(preset_row, 4, 0);
    lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(preset_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *preset_label = lv_label_create(preset_row);
    lv_label_set_text(preset_label, "Preset");
    lv_obj_set_style_text_color(preset_label, lv_color_white(), 0);
    lv_obj_set_width(preset_label, LV_PCT(100));

    ui_parameters.heater_profile_roller = create_heater_profile_roller(preset_row);
    create_heater_field_row(ui_parameters.group_heater, "Zieltemperatur (°C)", "Zieltemperatur des Presets (30 bis 120 °C).",
                            HEATER_FIELD_TARGET, LV_PCT(100));
    create_heater_field_row(ui_parameters.group_heater, "Hysterese (°C)", "Schaltabstand um den Sollwert (0.5 bis 5.0 °C).",
                            HEATER_FIELD_HYSTERESIS, LV_PCT(100));
    create_heater_field_row(ui_parameters.group_heater, "Anfahrband (°C)", "Fruehbereich vor dem Sollwert (1.0 bis 20.0 °C).",
                            HEATER_FIELD_APPROACH, LV_PCT(100));
    create_heater_field_row(ui_parameters.group_heater, "Halteband (°C)", "Band zum Halten der Temperatur (0.5 bis 10.0 °C).",
                            HEATER_FIELD_HOLD, LV_PCT(100));
    create_heater_field_row(ui_parameters.group_heater, "Ueberschwingen (°C)", "Maximal erlaubtes Ueberschwingen (0.5 bis 5.0 °C).",
                            HEATER_FIELD_OVERSHOOT, LV_PCT(100));
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
    lv_obj_align(ui_parameters.content_scroll, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui_parameters.content_scroll, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(ui_parameters.content_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_parameters.content_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_parameters.content_scroll, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_top(ui_parameters.content_scroll, 8, 0);
    lv_obj_set_style_pad_bottom(ui_parameters.content_scroll, 12, 0);
    lv_obj_set_style_pad_left(ui_parameters.content_scroll, 5, 0);
    lv_obj_set_style_pad_right(ui_parameters.content_scroll, 5, 0);
    lv_obj_set_style_pad_row(ui_parameters.content_scroll, 12, 0);
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

static void create_confirm_overlay(lv_obj_t *parent) {
    ui_parameters.confirm_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_parameters.confirm_overlay);
    lv_obj_set_size(ui_parameters.confirm_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ui_parameters.confirm_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_parameters.confirm_overlay, LV_OPA_60, 0);
    lv_obj_add_flag(ui_parameters.confirm_overlay, LV_OBJ_FLAG_HIDDEN);

    ui_parameters.confirm_dialog = lv_obj_create(ui_parameters.confirm_overlay);
    lv_obj_set_size(ui_parameters.confirm_dialog, 320, LV_SIZE_CONTENT);
    lv_obj_center(ui_parameters.confirm_dialog);
    lv_obj_set_style_radius(ui_parameters.confirm_dialog, 14, 0);
    lv_obj_set_style_border_width(ui_parameters.confirm_dialog, 1, 0);
    lv_obj_set_style_border_color(ui_parameters.confirm_dialog, col_hex(kColorStepperBorder), 0);
    lv_obj_set_style_bg_color(ui_parameters.confirm_dialog, col_hex(kColorCardBg), 0);
    lv_obj_set_style_bg_opa(ui_parameters.confirm_dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_parameters.confirm_dialog, 16, 0);
    lv_obj_set_style_pad_row(ui_parameters.confirm_dialog, 12, 0);
    lv_obj_set_flex_flow(ui_parameters.confirm_dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_parameters.confirm_dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    ui_parameters.confirm_text = lv_label_create(ui_parameters.confirm_dialog);
    lv_label_set_text(ui_parameters.confirm_text, "Aktuelle HOST-Parameter speichern?\nDas System startet danach neu.");
    lv_obj_set_width(ui_parameters.confirm_text, LV_PCT(100));
    lv_obj_set_style_text_color(ui_parameters.confirm_text, lv_color_white(), 0);
    lv_obj_set_style_text_align(ui_parameters.confirm_text, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *button_row = lv_obj_create(ui_parameters.confirm_dialog);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_width(button_row, LV_PCT(100));
    lv_obj_set_height(button_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(button_row, 12, 0);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_parameters.btn_confirm_cancel = lv_btn_create(button_row);
    lv_obj_set_size(ui_parameters.btn_confirm_cancel, 120, 42);
    style_small_button(ui_parameters.btn_confirm_cancel, 0x4A4A4A, 10);
    lv_obj_add_event_cb(ui_parameters.btn_confirm_cancel, confirm_cancel_event_cb, LV_EVENT_CLICKED, nullptr);
    ui_parameters.label_confirm_cancel = lv_label_create(ui_parameters.btn_confirm_cancel);
    lv_label_set_text(ui_parameters.label_confirm_cancel, "CANCEL");
    lv_obj_set_style_text_color(ui_parameters.label_confirm_cancel, lv_color_white(), 0);
    lv_obj_center(ui_parameters.label_confirm_cancel);

    ui_parameters.btn_confirm_save = lv_btn_create(button_row);
    lv_obj_set_size(ui_parameters.btn_confirm_save, 120, 42);
    style_small_button(ui_parameters.btn_confirm_save, kColorSave, 10);
    lv_obj_add_event_cb(ui_parameters.btn_confirm_save, confirm_save_event_cb, LV_EVENT_CLICKED, nullptr);
    ui_parameters.label_confirm_save = lv_label_create(ui_parameters.btn_confirm_save);
    lv_label_set_text(ui_parameters.label_confirm_save, "SAVE");
    lv_obj_set_style_text_color(ui_parameters.label_confirm_save, lv_color_white(), 0);
    lv_obj_center(ui_parameters.label_confirm_save);
}

static void set_info_message(const char *text, uint32_t color_hex) {
    if (!ui_parameters.label_info_message) {
        return;
    }
    lv_label_set_text(ui_parameters.label_info_message, text);
    lv_obj_set_style_text_color(ui_parameters.label_info_message, col_hex(color_hex), 0);
}

static uint16_t read_shortcut_value(lv_obj_t *roller) {
    return roller ? static_cast<uint16_t>(lv_roller_get_selected(roller)) : 0;
}

static void sync_shortcut_widgets_to_edit_parameters(void) {
    if (s_internal_update) {
        return;
    }
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        s_edit_parameters.shortcutPresetIds[i] = read_shortcut_value(ui_parameters.shortcut_roller[i]);
    }
}

static void sync_visible_heater_widgets_to_edit_parameters(void) {
}

static int16_t get_heater_field_value(const HostHeaterProfileParameters &profile, HeaterField field) {
    switch (field) {
        case HEATER_FIELD_TARGET: return profile.targetC;
        case HEATER_FIELD_HYSTERESIS: return profile.hysteresis_dC;
        case HEATER_FIELD_APPROACH: return profile.approachBand_dC;
        case HEATER_FIELD_HOLD: return profile.holdBand_dC;
        case HEATER_FIELD_OVERSHOOT: return profile.overshootCap_dC;
    }
    return 0;
}

static void set_heater_field_value(HostHeaterProfileParameters &profile, HeaterField field, int16_t value) {
    switch (field) {
        case HEATER_FIELD_TARGET: profile.targetC = value; break;
        case HEATER_FIELD_HYSTERESIS: profile.hysteresis_dC = value; break;
        case HEATER_FIELD_APPROACH: profile.approachBand_dC = value; break;
        case HEATER_FIELD_HOLD: profile.holdBand_dC = value; break;
        case HEATER_FIELD_OVERSHOOT: profile.overshootCap_dC = value; break;
    }
}

static int16_t clamp_heater_field_value(HeaterField field, int16_t value) {
    switch (field) {
        case HEATER_FIELD_TARGET:
            return static_cast<int16_t>(LV_CLAMP(30, value, 120));
        case HEATER_FIELD_HYSTERESIS:
        case HEATER_FIELD_OVERSHOOT:
            return static_cast<int16_t>(LV_CLAMP(5, value, 50));
        case HEATER_FIELD_APPROACH:
            return static_cast<int16_t>(LV_CLAMP(10, value, 200));
        case HEATER_FIELD_HOLD:
            return static_cast<int16_t>(LV_CLAMP(5, value, 100));
    }
    return value;
}

static int16_t heater_field_step(HeaterField field) {
    return (field == HEATER_FIELD_TARGET) ? 1 : 5;
}

static void format_heater_field_value(HeaterField field, int16_t value, char *out, size_t out_size) {
    if (field == HEATER_FIELD_TARGET) {
        std::snprintf(out, out_size, "%d", static_cast<int>(value));
        return;
    }

    const int whole = value / 10;
    const int decimal = std::abs(value % 10);
    std::snprintf(out, out_size, "%d.%d", whole, decimal);
}

static void update_heater_field_display(HeaterField field) {
    lv_obj_t *label = ui_parameters.heater_value_label[field];
    if (!label) {
        return;
    }

    char value_text[8];
    format_heater_field_value(field,
                              get_heater_field_value(s_edit_parameters.heaterProfiles[s_selected_heater_profile], field),
                              value_text, sizeof(value_text));
    lv_label_set_text(label, value_text);
    lv_obj_center(label);
}

static void load_selected_heater_profile_into_widgets(void) {
    s_internal_update = true;
    for (uint8_t field = 0; field < UI_PARAMETER_HEATER_FIELD_COUNT; ++field) {
        update_heater_field_display(static_cast<HeaterField>(field));
    }
    s_internal_update = false;
}

static bool heater_profile_equals(const HeaterProfileUiState &lhs, const HeaterProfileUiState &rhs) {
    return lhs.targetC == rhs.targetC &&
           lhs.hysteresis_dC == rhs.hysteresis_dC &&
           lhs.approachBand_dC == rhs.approachBand_dC &&
           lhs.holdBand_dC == rhs.holdBand_dC &&
           lhs.overshootCap_dC == rhs.overshootCap_dC;
}

static bool screen_has_unsaved_changes(void) {
    if (!s_internal_update) {
        sync_shortcut_widgets_to_edit_parameters();
        sync_visible_heater_widgets_to_edit_parameters();
    }

    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        if (s_edit_parameters.shortcutPresetIds[i] != s_saved_parameters.shortcutPresetIds[i]) {
            return true;
        }
    }

    for (uint8_t i = 0; i < UI_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        HeaterProfileUiState current_profile = {
            s_edit_parameters.heaterProfiles[i].targetC,
            s_edit_parameters.heaterProfiles[i].hysteresis_dC,
            s_edit_parameters.heaterProfiles[i].approachBand_dC,
            s_edit_parameters.heaterProfiles[i].holdBand_dC,
            s_edit_parameters.heaterProfiles[i].overshootCap_dC,
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
    if (!ui_parameters.btn_save) {
        return;
    }
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
    s_edit_parameters = defaults;

    s_internal_update = true;
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_roller_set_selected(ui_parameters.shortcut_roller[i], defaults.shortcutPresetIds[i], LV_ANIM_OFF);
    }
    lv_roller_set_selected(ui_parameters.heater_profile_roller, s_selected_heater_profile, LV_ANIM_OFF);
    s_internal_update = false;
    load_selected_heater_profile_into_widgets();
    update_save_button_state();
}

static void load_saved_state_into_widgets(void) {
    s_edit_parameters = s_saved_parameters;
    s_internal_update = true;
    for (uint8_t i = 0; i < UI_PARAMETER_SHORTCUT_SLOT_COUNT; ++i) {
        lv_roller_set_selected(ui_parameters.shortcut_roller[i], s_saved_parameters.shortcutPresetIds[i], LV_ANIM_OFF);
    }
    lv_roller_set_selected(ui_parameters.heater_profile_roller, s_selected_heater_profile, LV_ANIM_OFF);
    s_internal_update = false;
    load_selected_heater_profile_into_widgets();
    update_save_button_state();
}

static void spinbox_increment_event_cb(lv_event_t *e) {
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const uintptr_t user_data = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    const HeaterField field = static_cast<HeaterField>(user_data & 0x7F);
    HostHeaterProfileParameters &profile = s_edit_parameters.heaterProfiles[s_selected_heater_profile];
    const int16_t next_value = clamp_heater_field_value(field,
                                                        static_cast<int16_t>(get_heater_field_value(profile, field) +
                                                                             heater_field_step(field)));
    set_heater_field_value(profile, field, next_value);
    update_heater_field_display(field);
    spinbox_value_changed_cb(e);
}

static void spinbox_decrement_event_cb(lv_event_t *e) {
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const uintptr_t user_data = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    const HeaterField field = static_cast<HeaterField>(user_data & 0x7F);
    HostHeaterProfileParameters &profile = s_edit_parameters.heaterProfiles[s_selected_heater_profile];
    const int16_t next_value = clamp_heater_field_value(field,
                                                        static_cast<int16_t>(get_heater_field_value(profile, field) -
                                                                             heater_field_step(field)));
    set_heater_field_value(profile, field, next_value);
    update_heater_field_display(field);
    spinbox_value_changed_cb(e);
}

static void heater_profile_roller_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    if (!ui_parameters.heater_profile_roller) {
        return;
    }
    sync_visible_heater_widgets_to_edit_parameters();
    s_selected_heater_profile = static_cast<uint8_t>(lv_roller_get_selected(ui_parameters.heater_profile_roller));
    load_selected_heater_profile_into_widgets();
    update_save_button_state();
    if (!s_internal_update) {
        set_info_message("Preset gewechselt", 0xA8A8A8);
    }
}

static void spinbox_value_changed_cb(lv_event_t *e) {
    LV_UNUSED(e);
    if (s_internal_update) {
        return;
    }
    sync_shortcut_widgets_to_edit_parameters();
    sync_visible_heater_widgets_to_edit_parameters();
    update_save_button_state();
    if (!s_internal_update) {
        set_info_message("Nicht gespeicherte Aenderungen", 0xF0B040);
    }
}

static void button_reset_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    show_confirm_overlay(CONFIRM_ACTION_RESET);
}

static void button_save_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    sync_shortcut_widgets_to_edit_parameters();
    show_confirm_overlay(CONFIRM_ACTION_SAVE);
}

static void confirm_cancel_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    hide_confirm_overlay();
}

static void confirm_save_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    const ConfirmAction action = s_confirm_action;
    hide_confirm_overlay();
    switch (action) {
        case CONFIRM_ACTION_SAVE:
            sync_shortcut_widgets_to_edit_parameters();
            save_parameters_and_reboot(s_edit_parameters);
            break;
        case CONFIRM_ACTION_RESET:
            reset_widgets_to_defaults();
            save_parameters_and_reboot(s_edit_parameters);
            break;
        case CONFIRM_ACTION_NONE:
        default:
            break;
    }
}

static void hide_confirm_overlay(void) {
    if (ui_parameters.confirm_overlay) {
        lv_obj_add_flag(ui_parameters.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    s_confirm_action = CONFIRM_ACTION_NONE;
}

static void show_confirm_overlay(ConfirmAction action) {
    if (!ui_parameters.confirm_overlay) {
        return;
    }

    s_confirm_action = action;

    switch (action) {
        case CONFIRM_ACTION_RESET:
            lv_label_set_text(ui_parameters.confirm_text, "Werkeinstellungen laden?\nDas System startet danach neu.");
            lv_label_set_text(ui_parameters.label_confirm_cancel, "NO");
            lv_label_set_text(ui_parameters.label_confirm_save, "YES");
            lv_obj_set_style_bg_color(ui_parameters.btn_confirm_save, col_hex(kColorReset), 0);
            break;
        case CONFIRM_ACTION_SAVE:
        default:
            lv_label_set_text(ui_parameters.confirm_text, "Aktuelle HOST-Parameter speichern?\nDas System startet danach neu.");
            lv_label_set_text(ui_parameters.label_confirm_cancel, "CANCEL");
            lv_label_set_text(ui_parameters.label_confirm_save, "SAVE");
            lv_obj_set_style_bg_color(ui_parameters.btn_confirm_save, col_hex(kColorSave), 0);
            break;
    }

    lv_obj_clear_flag(ui_parameters.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void save_parameters_and_reboot(const HostParameters &params) {
    HostParameters candidate = params;
    if (!host_parameters_save(&candidate)) {
        set_info_message("SAVE fehlgeschlagen", 0xFF7070);
        return;
    }

    s_saved_parameters = candidate;
    update_save_button_state();
    set_info_message("Gespeichert, Neustart...", 0x70D070);
    delay(120);
    ESP.restart();
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
    create_confirm_overlay(ui_parameters.root);

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
