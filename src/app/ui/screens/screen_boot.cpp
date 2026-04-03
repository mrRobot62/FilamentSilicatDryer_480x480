#include "screen_boot.h"

#include "screen_base.h"

#include <cstdio>

namespace {

struct boot_screen_widgets_t {
    lv_obj_t *root = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_obj_t *progress_bar = nullptr;
    lv_obj_t *percent_label = nullptr;
};

boot_screen_widgets_t ui;

constexpr int kBarWidth = 360;
constexpr int kBarHeight = 18;
constexpr int kBarYOffset = 120;
constexpr int kStatusYOffset = 78;
constexpr uint32_t kBootBarBgHex = 0x404040;

} // namespace

lv_obj_t *screen_boot_create(lv_obj_t *parent) {
    if (ui.root) {
        return ui.root;
    }

    ui.root = lv_obj_create(parent);
    lv_obj_remove_style_all(ui.root);
    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);
    lv_obj_set_style_bg_color(ui.root, ui_color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    ui.status_label = lv_label_create(ui.root);
    lv_label_set_text(ui.status_label, "Systemstart...");
    lv_obj_set_style_text_color(ui.status_label, ui_color_from_hex(UI_COLOR_WHITE_FG_HEX), 0);
    lv_obj_align(ui.status_label, LV_ALIGN_CENTER, 0, kStatusYOffset);

    ui.progress_bar = lv_bar_create(ui.root);
    lv_obj_set_size(ui.progress_bar, kBarWidth, kBarHeight);
    lv_obj_align(ui.progress_bar, LV_ALIGN_CENTER, 0, kBarYOffset);
    lv_bar_set_range(ui.progress_bar, 0, 100);
    lv_bar_set_value(ui.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.progress_bar, ui_color_from_hex(kBootBarBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.progress_bar, ui_color_from_hex(UI_COLOR_TIME_BAR_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui.progress_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.progress_bar, 6, LV_PART_INDICATOR);

    ui.percent_label = lv_label_create(ui.root);
    lv_label_set_text(ui.percent_label, "0 %");
    lv_obj_set_style_text_color(ui.percent_label, ui_color_from_hex(UI_COLOR_WHITE_FG_HEX), 0);
    lv_obj_align_to(ui.percent_label, ui.progress_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    return ui.root;
}

lv_obj_t *screen_boot_get_swipe_target(void) {
    return nullptr;
}

void screen_boot_set_progress(uint8_t percent) {
    if (!ui.progress_bar || !ui.percent_label) {
        return;
    }

    if (percent > 100) {
        percent = 100;
    }

    char buf[8];
    lv_bar_set_value(ui.progress_bar, percent, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%u %%", (unsigned)percent);
    lv_label_set_text(ui.percent_label, buf);
}

void screen_boot_set_status(const char *text) {
    if (!ui.status_label) {
        return;
    }

    lv_label_set_text(ui.status_label, (text && text[0]) ? text : "");
}
