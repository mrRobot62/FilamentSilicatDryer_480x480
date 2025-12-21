#pragma once
#include <lvgl.h>
#include "log_ui.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ScreenBaseLayout
    {
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

#ifdef __cplusplus
}
#endif