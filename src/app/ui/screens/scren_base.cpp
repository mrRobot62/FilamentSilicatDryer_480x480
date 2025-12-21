#include "screen_base.h"

static void style_root_black(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_transparent(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void screen_base_create(ScreenBaseLayout *out, lv_obj_t *parent,
                        int screen_w, int screen_h,
                        int top_h, int page_h, int bottom_h,
                        int side_w, int center_w)
{
    if (!out)
        return;

    // Root
    out->root = lv_obj_create(parent);
    style_root_black(out->root);
    lv_obj_set_size(out->root, screen_w, screen_h);
    lv_obj_center(out->root);

    // TOP
    out->top = lv_obj_create(out->root);
    style_transparent(out->top);
    lv_obj_set_size(out->top, screen_w, top_h);
    lv_obj_align(out->top, LV_ALIGN_TOP_MID, 0, 0);

    // BOTTOM
    out->bottom = lv_obj_create(out->root);
    style_transparent(out->bottom);
    lv_obj_set_size(out->bottom, screen_w, bottom_h);
    lv_obj_align(out->bottom, LV_ALIGN_BOTTOM_MID, 0, 0);

    // PAGE INDICATOR (Swipe zone)
    out->page_indicator = lv_obj_create(out->root);
    style_transparent(out->page_indicator);
    lv_obj_set_size(out->page_indicator, center_w, page_h);
    lv_obj_align(out->page_indicator, LV_ALIGN_BOTTOM_MID, 0, -bottom_h);

    // CENTER (between top and page_indicator)
    const int center_h = screen_h - top_h - page_h - bottom_h;

    out->center = lv_obj_create(out->root);
    style_transparent(out->center);
    lv_obj_set_size(out->center, screen_w, center_h);
    lv_obj_align(out->center, LV_ALIGN_TOP_MID, 0, top_h);

    // LEFT / MIDDLE / RIGHT inside center
    out->left = lv_obj_create(out->center);
    style_transparent(out->left);
    lv_obj_set_size(out->left, side_w, center_h);
    lv_obj_align(out->left, LV_ALIGN_LEFT_MID, 0, 0);

    out->right = lv_obj_create(out->center);
    style_transparent(out->right);
    lv_obj_set_size(out->right, side_w, center_h);
    lv_obj_align(out->right, LV_ALIGN_RIGHT_MID, 0, 0);

    out->middle = lv_obj_create(out->center);
    style_transparent(out->middle);
    lv_obj_set_size(out->middle, center_w, center_h);
    lv_obj_align(out->middle, LV_ALIGN_CENTER, 0, 0);
}