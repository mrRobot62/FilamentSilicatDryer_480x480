#pragma once

#include <cstdio>
#include <cstring>
#include <lvgl.h>

#include "oven.h"
#include "screen_base.h"
#include "screen_manager.h"

static constexpr uint8_t UI_PARAMETER_SHORTCUT_SLOT_COUNT = 4;
static constexpr uint8_t UI_PARAMETER_HEATER_PROFILE_COUNT = 4;
static constexpr uint8_t UI_PARAMETER_HEATER_FIELD_COUNT = 5;

typedef struct parameters_screen_widgets_t {
    lv_obj_t *root;

    lv_obj_t *top_bar_container;
    lv_obj_t *center_container;
    lv_obj_t *page_indicator_container;
    lv_obj_t *bottom_container;
    lv_obj_t *s_swipe_target;

    lv_obj_t *icons_container;
    lv_obj_t *config_container;
    lv_obj_t *button_container;

    lv_obj_t *label_title;
    lv_obj_t *content_scroll;
    lv_obj_t *label_info_message;

    lv_obj_t *group_shortcuts;
    lv_obj_t *group_heater;

    lv_obj_t *shortcut_button[UI_PARAMETER_SHORTCUT_SLOT_COUNT];
    lv_obj_t *shortcut_roller[UI_PARAMETER_SHORTCUT_SLOT_COUNT];

    lv_obj_t *heater_profile_roller;
    lv_obj_t *heater_value_label[UI_PARAMETER_HEATER_FIELD_COUNT];

    lv_obj_t *btn_reset;
    lv_obj_t *btn_save;
    lv_obj_t *label_btn_reset;
    lv_obj_t *label_btn_save;

    lv_obj_t *confirm_overlay;
    lv_obj_t *confirm_dialog;
    lv_obj_t *confirm_text;
    lv_obj_t *btn_confirm_cancel;
    lv_obj_t *btn_confirm_save;
    lv_obj_t *label_confirm_cancel;
    lv_obj_t *label_confirm_save;

    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];
} parameters_screen_widgets_t;

static parameters_screen_widgets_t ui_parameters;

lv_obj_t *screen_parameters_create(lv_obj_t *parent);
lv_obj_t *screen_parameters_get_swipe_target(void);
void screen_parameters_set_active_page(uint8_t page_index);
