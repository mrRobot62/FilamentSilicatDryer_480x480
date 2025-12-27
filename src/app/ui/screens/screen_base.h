#pragma once
#include "log_ui.h"
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScreenBaseLayout {
    lv_obj_t *root;

    lv_obj_t *top;
    lv_obj_t *center;

    lv_obj_t *left;
    lv_obj_t *middle;
    lv_obj_t *right;

    lv_obj_t *page_indicator; // swipe zone lives here
    lv_obj_t *bottom;
} ScreenBaseLayout;

void screen_base_create(ScreenBaseLayout *out, lv_obj_t *parent,
                        int screen_w, int screen_h,
                        int top_h, int page_h, int bottom_h,
                        int side_w, int center_w);

// Helpers
// UI color helper: swaps R <-> B (because the panel path swaps channels)
static lv_color_t ui_color_from_hex(uint32_t rgb_hex) {
    // rgb_hex is 0xRRGGBB
    uint32_t r = (rgb_hex >> 16) & 0xFF;
    uint32_t g = (rgb_hex >> 8) & 0xFF;
    uint32_t b = (rgb_hex >> 0) & 0xFF;

    uint32_t swapped = (b << 16) | (g << 8) | (r << 0); // 0xBBGGRR
    return lv_color_hex(swapped);
}

#ifdef __cplusplus
}
#endif