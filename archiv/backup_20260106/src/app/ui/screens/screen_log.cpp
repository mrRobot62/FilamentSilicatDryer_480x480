#include "screen_log.h"

#include "screen_base.h"

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static void create_page_indicator(lv_obj_t *parent);
static void create_top_placeholder(lv_obj_t *parent);
static void create_bottom_placeholder(lv_obj_t *parent);

constexpr int LV_OPA_15 = 15;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
lv_obj_t *screen_log_create(lv_obj_t *parent)
{
    if (ui_log.root != nullptr)
    {
        UI_INFO("[screen_log] reusing existing root\n");
        return ui_log.root;
    }

    ScreenBaseLayout base{};
    screen_base_create(&base, parent,
                       UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT,
                       60, 60, 60, // top_h, page_h, bottom_h
                       60, 360);   // side_w, center_w

    // Map base layout containers to ui_log struct
    ui_log.root = base.root;

    ui_log.top_bar_container = base.top;
    ui_log.center_container = base.center;
    ui_log.page_indicator_container = base.page_indicator;
    ui_log.bottom_container = base.bottom;

    ui_log.icons_container = base.left;
    ui_log.config_container = base.middle;
    ui_log.button_container = base.right;

    // Create page indicator (also swipe zone)
    create_page_indicator(ui_log.page_indicator_container);

    // Simple placeholders so we can see the screen
    create_top_placeholder(ui_log.top_bar_container);
    create_bottom_placeholder(ui_log.bottom_container);

    UI_INFO("[screen_log] created root=%p swipe_target=%p\n",
            (void *)ui_log.root, (void *)ui_log.s_swipe_target);

    return ui_log.root;
}

lv_obj_t *screen_log_get_swipe_target(void)
{
    return ui_log.s_swipe_target;
}

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static void create_page_indicator(lv_obj_t *parent)
{
    // Use the whole page indicator container as swipe zone (360px wide via base layout)
    ui_log.s_swipe_target = parent;

    // Must receive events
    lv_obj_add_flag(ui_log.s_swipe_target, LV_OBJ_FLAG_CLICKABLE);

    // Very subtle hint background
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_15, 0);
    lv_obj_set_style_radius(parent, 10, 0);

    // Inner panel + dots (optional but keeps UI consistent)
    ui_log.page_indicator_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(ui_log.page_indicator_panel);
    lv_obj_set_size(ui_log.page_indicator_panel, 100, 24);
    lv_obj_center(ui_log.page_indicator_panel);

    lv_obj_set_style_radius(ui_log.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui_log.page_indicator_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(ui_log.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_log.page_indicator_panel, 4, 0);

    lv_obj_set_flex_flow(ui_log.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_log.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        ui_log.page_dots[i] = lv_obj_create(ui_log.page_indicator_panel);
        lv_obj_remove_style_all(ui_log.page_dots[i]);
        lv_obj_set_size(ui_log.page_dots[i], 10, 10);
        lv_obj_set_style_radius(ui_log.page_dots[i], 5, 0);
        lv_obj_set_style_bg_opa(ui_log.page_dots[i], LV_OPA_COVER, 0);

        // LOG is page index 2 (0=main, 1=config, 2=log)
        lv_obj_set_style_bg_color(ui_log.page_dots[i],
                                  (i == 2) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x606060), 0);

        if (i > 0)
            lv_obj_set_style_margin_left(ui_log.page_dots[i], 8, 0);
    }
}

static void create_top_placeholder(lv_obj_t *parent)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "LOG");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

static void create_bottom_placeholder(lv_obj_t *parent)
{
    ui_log.label_info_message = lv_label_create(parent);
    lv_label_set_text(ui_log.label_info_message, "Swipe here to change screens");
    lv_obj_set_style_text_color(ui_log.label_info_message, lv_color_hex(0xB0B0B0), 0);
    lv_obj_center(ui_log.label_info_message);
}