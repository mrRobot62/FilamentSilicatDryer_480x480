#pragma once
#include <lvgl.h>
#pragma once
#include "screen_base.h"
#include "screen_manager.h"
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif
// Layout constants (geometry)
#define UI_SCREEN_WIDTH 480
#define UI_SCREEN_HEIGHT 480

#define UI_SIDE_PADDING 60
#define UI_TOP_PADDING 5
#define UI_BOTTOM_PADDING 5
#define UI_FRAME_PADDING 10
typedef struct log_screen_widgets_t {
    lv_obj_t *root;

    // --------------------------------------------------------
    // Containers
    // --------------------------------------------------------
    lv_obj_t *top_bar_container;
    lv_obj_t *center_container;
    lv_obj_t *page_indicator_container;
    lv_obj_t *bottom_container;

    lv_obj_t *swipe_hit;
    lv_obj_t *s_swipe_target;
    // --------------------------------------------------------
    // Top bar
    // --------------------------------------------------------

    // --------------------------------------------------------
    // Center
    // --------------------------------------------------------
    lv_obj_t *icons_container;
    lv_obj_t *config_container;
    lv_obj_t *button_container;

    // --------------------------------------------------------
    // Icons
    // --------------------------------------------------------
    // --------------------------------------------------------
    // Icons
    // Icons are used to give a chance to make a runtime Preset
    // configuration (e.g. enable/disable heater, fan, motor, lamp)
    //
    // Simple example:
    // you want to use for specific unkonw setup without heater but lamp should be on allways
    // --------------------------------------------------------
    lv_obj_t *icon_fan12v;      // only visible not changable
    lv_obj_t *icon_fan230;      // user toggleable
    lv_obj_t *icon_fan230_slow; // user toggleable
    lv_obj_t *icon_heater;      // user toggleable
    lv_obj_t *icon_door;        // only visible not changable
    lv_obj_t *icon_motor;       // user toggleable
    lv_obj_t *icon_lamp;        // user toggleable

    // --------------------------------------------------------
    // Widgets
    // --------------------------------------------------------
    lv_obj_t *log_table;

    // --------------------------------------------------------
    // Buttons
    // --------------------------------------------------------
    lv_obj_t *btn_clear;

    // --------------------------------------------------------
    // Page indicator
    // --------------------------------------------------------
    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];

    // --------------------------------------------------------
    // Bottom - Info Message
    // --------------------------------------------------------
    lv_obj_t *label_info_message;

} log_screen_widgets_t;

static log_screen_widgets_t ui_log;

lv_obj_t *screen_log_create(lv_obj_t *parent);
lv_obj_t *screen_log_get_swipe_target(void);
void screen_log_set_active_page(uint8_t page_index);

#ifdef __cplusplus
} // extern "C"
#endif