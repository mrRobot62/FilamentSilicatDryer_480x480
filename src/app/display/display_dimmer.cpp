#include "display/display_dimmer.h"

#include <lvgl.h>

namespace {

static lv_obj_t *g_dim_overlay = nullptr;
static uint8_t g_brightness_percent = 100;

static lv_opa_t opacity_from_brightness(uint8_t percent) {
    if (percent >= 100) {
        return LV_OPA_TRANSP;
    }
    const uint32_t hidden_percent = 100u - percent;
    return static_cast<lv_opa_t>((hidden_percent * LV_OPA_COVER) / 100u);
}

} // namespace

void display_dimmer_init(void) {
    if (g_dim_overlay) {
        return;
    }

    g_dim_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_dim_overlay);
    lv_obj_set_size(g_dim_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_dim_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_dim_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_dim_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_dim_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_dim_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(g_dim_overlay, LV_OBJ_FLAG_HIDDEN);
}

void display_dimmer_set_brightness_percent(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    g_brightness_percent = percent;

    if (!g_dim_overlay && percent >= 100) {
        return;
    }

    if (!g_dim_overlay) {
        display_dimmer_init();
    }

    const lv_opa_t opa = opacity_from_brightness(percent);
    lv_obj_set_style_bg_opa(g_dim_overlay, opa, 0);

    if (opa == LV_OPA_TRANSP) {
        lv_obj_add_flag(g_dim_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(g_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_dim_overlay);
    }
}

uint8_t display_dimmer_get_brightness_percent(void) {
    return g_brightness_percent;
}
