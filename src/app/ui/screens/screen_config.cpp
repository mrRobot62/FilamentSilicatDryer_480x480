#include "screen_config.h"
#include "log_scr_conf.h"

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

lv_obj_t *screen_config_create(lv_obj_t *parent)
{
    if (ui_config.root)
        return ui_config.root;

    ui_config.root = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_config.root);
    lv_obj_set_size(ui_config.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(ui_config.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui_config.root, lv_color_black(), 0);
    lv_obj_clear_flag(ui_config.root, LV_OBJ_FLAG_SCROLLABLE);

    // Swipe hit area (bottom center, 360px wide)
    ui_config.s_swipe_target =create_swipe_hint(ui_config.root);

        // Optional label for clarity
        lv_obj_t *lbl = lv_label_create(ui_config.root);
    lv_label_set_text(lbl, "CONFIG");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 12);
    INFO("[SCR_CONF] created. screen-addr: %d\n", ui_config.root);

    return ui_config.root;
}

lv_obj_t *screen_config_get_swipe_target(void)
{
    return ui_config.s_swipe_target;
}