#include "screen_log.h"
#include "log_core.h"

static lv_obj_t *create_swipe_hint(lv_obj_t *parent)
{
    lv_obj_t *hit = lv_obj_create(parent);
    lv_obj_remove_style_all(hit);

    lv_obj_set_size(hit, 360, 60);
    lv_obj_align(hit, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Very subtle hint (same everywhere)
    lv_obj_set_style_bg_color(hit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(hit, 10, 0); // subtle
    lv_obj_set_style_radius(hit, 8, 0);

    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    return hit;
}

lv_obj_t *screen_log_create(lv_obj_t *parent)
{
    if (ui_log.root)
        return ui_log.root;

    DBG("[SCR_LOG]  %d\n", 1);
    ui_log.root = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_log.root);
    lv_obj_set_size(ui_log.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(ui_log.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui_log.root, lv_color_black(), 0);
    lv_obj_clear_flag(ui_log.root, LV_OBJ_FLAG_SCROLLABLE);

    // Swipe hit area (bottom center, 360px wide)
    ui_log.s_swipe_target = create_swipe_hint(ui_log.root);

    // Optional label for clarity
    lv_obj_t *lbl = lv_label_create(ui_log.root);
    lv_label_set_text(lbl, "UI_LOG");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 12);

    INFO("[SCR_LOG] created. screen-addr: %d\n", ui_log.root);

    return ui_log.root;
}

lv_obj_t *screen_log_get_swipe_target(void)
{
    return ui_log.s_swipe_target;
}