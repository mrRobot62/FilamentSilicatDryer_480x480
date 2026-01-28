#include "screen_dbg_hw.h"

#include "log_ui.h"
#include "oven_utils.h"  // includes oven.h
#include "screen_base.h" // includes ui_color_constants.h + ui_general_screen_constants.h

#include "../icons/icons_32x32.h"

#include <cmath> // lroundf
#include <cstdio>
#include <cstring>

namespace {

// ----------------------------------------------------------------------------
// Local UI-only gate (allowed by spec: RUN is purely a UI safety gate)
// ----------------------------------------------------------------------------
static bool g_run_gate = false;

// ----------------------------------------------------------------------------
// Widget storage
// ----------------------------------------------------------------------------
typedef struct dbg_hw_screen_widgets_t {
    lv_obj_t *root;
    ScreenBaseLayout base;

    // --- Top ---
    lv_obj_t *icon_sync;

    // --- Left icons ---
    lv_obj_t *icons_container;
    lv_obj_t *icon[7];

    // --- Middle content rows ---
    lv_obj_t *rows_container;
    lv_obj_t *row[HW_ROWS];
    lv_obj_t *row_label[HW_ROWS];

    // --- Right buttons ---
    lv_obj_t *btn_run;
    lv_obj_t *lbl_run;

    lv_obj_t *btn_clear;
    // lv_obj_t *lbl_clear;

    // --- Page indicator ---
    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];
    lv_obj_t *swipe_hit;      // dedicated swipe target
    lv_obj_t *s_swipe_target; // exported to screen_manager

    // --- Bottom temp ---
    lv_obj_t *temp_scale;
    lv_obj_t *temp_tri_target;
    lv_obj_t *temp_tri_current;
    lv_obj_t *temp_label_target;
    lv_obj_t *temp_label_current;

    // line connect - icon to row container

    lv_obj_t *rows[HW_ROWS];
    lv_obj_t *lines[HW_ROWS];
    lv_point_precise_t line_pts[HW_ROWS][3];

} dbg_hw_screen_widgets_t;

static dbg_hw_screen_widgets_t ui;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static inline lv_color_t col_hex(uint32_t hex) { return ui_color_from_hex(hex); }

static lv_point_precise_t obj_center(lv_obj_t *o) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    return {(lv_coord_t)((a.x1 + a.x2) / 2), (lv_coord_t)((a.y1 + a.y2) / 2)};
}

static lv_point_precise_t obj_left_center(lv_obj_t *o) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    return {(lv_coord_t)(a.x1), (lv_coord_t)((a.y1 + a.y2) / 2)};
}

static lv_point_precise_t to_line_local(lv_obj_t *line, lv_point_precise_t p_abs) {
    lv_area_t a;
    lv_obj_get_coords(line, &a);
    p_abs.x -= a.x1;
    p_abs.y -= a.y1;
    return p_abs;
}

static void update_wire(uint8_t i, lv_obj_t *icon, lv_obj_t *row, bool on) {
    lv_point_precise_t p0 = obj_center(icon);
    lv_point_precise_t p2 = obj_left_center(row);

    // bring endpoint slightly inside row
    p2.x += 2;

    lv_point_precise_t p1;
    p1.x = (lv_coord_t)(p2.x - 12);
    p1.y = p0.y;

    // --- NEW: convert absolute -> line local ---
    p0 = to_line_local(ui.lines[i], p0);
    p1 = to_line_local(ui.lines[i], p1);
    p2 = to_line_local(ui.lines[i], p2);

    ui.line_pts[i][0] = p0;
    ui.line_pts[i][1] = p1;
    ui.line_pts[i][2] = p2;

    lv_line_set_points(ui.lines[i], ui.line_pts[i], 3);

    const uint32_t colv = on ? UI_COLOR_ICON_ON_HEX : UI_COLOR_ICON_OFF_HEX;
    lv_color_t col = ui_color_from_hex(colv);

    lv_obj_set_style_line_color(ui.lines[i], col, LV_PART_MAIN);
    lv_obj_set_style_line_opa(ui.lines[i], LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, col, LV_PART_MAIN);
    lv_obj_set_style_border_opa(row, LV_OPA_50, LV_PART_MAIN);
}

static void icon_link_synced(lv_obj_t *link_icon) {
    if (!link_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(link_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(link_icon, col_hex(UI_COLOR_LINK_SYNCED), LV_PART_MAIN);
}

static void icon_link_unsynced(lv_obj_t *link_icon) {
    if (!link_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(link_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(link_icon, col_hex(UI_COLOR_DANGER_HEX), LV_PART_MAIN);
}

static void set_icon_onoff(lv_obj_t *obj, bool on) {
    if (!obj) {
        return;
    }

    if (on) {
        lv_obj_set_style_img_recolor(obj, col_hex(UI_COLOR_ICON_ON_HEX), LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        // OFF = original white (no recolor)
        lv_obj_set_style_img_recolor_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    }
}

static void set_row_onoff(lv_obj_t *row_obj, bool on) {
    if (!row_obj) {
        return;
    }

    if (on) {
        lv_obj_set_style_bg_color(row_obj, col_hex(UI_COLOR_ICON_ON_HEX), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row_obj, LV_OPA_60, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(row_obj, col_hex(UI_COLOR_BG_HEX), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row_obj, LV_OPA_TRANSP, LV_PART_MAIN);
    }
}

static void set_run_button_ui(void) {
    if (!ui.btn_run || !ui.lbl_run) {
        return;
    }

    // Spec: RUN=false => orange, RUN=true => red
    const uint32_t bg = g_run_gate ? UI_COLOR_DANGER_HEX : 0xFFA500;
    lv_obj_set_style_bg_color(ui.btn_run, col_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_run, LV_OPA_COVER, LV_PART_MAIN);

    // disabled look when RUN=false
    if (!g_run_gate) {
        lv_obj_set_style_opa(ui.btn_run, LV_OPA_80, LV_PART_MAIN);
    } else {
        lv_obj_set_style_opa(ui.btn_run, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static bool get_port_on_by_index(const OvenRuntimeState &st, int idx) {
    switch (idx) {
    case 0:
        return st.fan12v_on;
    case 1:
        return st.fan230_on;
    case 2:
        return st.fan230_slow_on;
    case 3:
        return st.heater_on;
    case 4:
        return st.door_open; // sensor truth as reported
    case 5:
        return st.motor_on;
    case 6:
        return st.lamp_on;
    default:
        return false;
    }
}

static const char *get_port_name_by_index(int idx) {
    switch (idx) {
    case 0:
        return "FAN12V";
    case 1:
        return "FAN230V";
    case 2:
        return "FAN230V_SLOW";
    case 3:
        return "HEATER";
    case 4:
        return "DOOR";
    case 5:
        return "MOTOR";
    case 6:
        return "LAMP";
    default:
        return "UNKNOWN";
    }
}

static void do_clear_ui_and_outputs(void) {
    // Force RUN=false
    g_run_gate = false;
    set_run_button_ui();

    // Clear content texts
    for (int i = 0; i < 7; ++i) {
        if (ui.row_label[i]) {
            lv_label_set_text(ui.row_label[i], "");
        }
    }

    // Clear remote outputs (this is what makes icons go white after ACK/STATUS)
    oven_force_outputs_off();

    UI_INFO("[DBG_HW] CLEAR (RUN forced false, outputs cleared)\n");
}

// ----------------------------------------------------------------------------
// Events
// ----------------------------------------------------------------------------
static void icon_click_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    auto *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (!g_run_gate) {
        UI_INFO("[DBG_HW] Icon click ignored (RUN=false) idx=%d\n", idx);
        return;
    }
    oven_dbg_hw_toggle_by_index(idx);
    UI_INFO("[DBG_HW] Icon clicked idx=%d obj=%p (armed)\n", idx, (void *)target);
}

// static void run_button_event_cb(lv_event_t *e) {
//     if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
//         return;
//     }

//     g_run_gate = !g_run_gate;
//     UI_INFO("[DBG_HW] RUN gate=%d\n", g_run_gate ? 1 : 0);

//     set_run_button_ui();
// }

static void run_button_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    g_run_gate = !g_run_gate;
    UI_INFO("[DBG_HW] RUN gate=%d\n", g_run_gate ? 1 : 0);

    set_run_button_ui();

    // When turning RUN OFF, behave like CLEAR (your expected behavior)
    if (!g_run_gate) {
        do_clear_ui_and_outputs();
    }
}

// static void clear_button_event_cb(lv_event_t *e) {
//     if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
//         return;
//     }
//     do_clear_ui_and_outputs();
// }

// ----------------------------------------------------------------------------
// Bottom temperature helpers (minimal clone of screen_main logic)
// ----------------------------------------------------------------------------
static uint32_t temp_status_color_hex(float cur, float tgt) {
    const float lo = tgt - (float)ui_temp_target_tolerance_c;
    const float hi = tgt + (float)ui_temp_target_tolerance_c;

    if (cur < lo) {
        return UI_COLOR_TEMP_COLD_HEX;
    }
    if (cur > hi) {
        return UI_COLOR_TEMP_HOT_HEX;
    }
    return UI_COLOR_TEMP_OK_HEX;
}

static void update_temp_ui(const OvenRuntimeState &state) {
    if (!ui.temp_scale) {
        return;
    }

    float cur_f = state.tempCurrent;
    float tgt_f = state.tempTarget;

    int16_t cur = (int16_t)lroundf(cur_f);
    int16_t tgt = (int16_t)lroundf(tgt_f);

    if (cur < UI_TEMP_MIN_C) {
        cur = UI_TEMP_MIN_C;
    }
    if (cur > UI_TEMP_MAX_C) {
        cur = UI_TEMP_MAX_C;
    }
    if (tgt < UI_TEMP_MIN_C) {
        tgt = UI_TEMP_MIN_C;
    }
    if (tgt > UI_TEMP_MAX_C) {
        tgt = UI_TEMP_MAX_C;
    }

    // Labels
    char buf_cur[16];
    std::snprintf(buf_cur, sizeof(buf_cur), "%d °C", (int)cur);
    lv_label_set_text(ui.temp_label_current, buf_cur);

    char buf_tgt[16];
    std::snprintf(buf_tgt, sizeof(buf_tgt), "%d °C", (int)tgt);
    lv_label_set_text(ui.temp_label_target, buf_tgt);

    // Dim target label when in range
    {
        const int tol = ui_temp_target_tolerance_c;
        const bool in_range = (cur >= (tgt - tol)) && (cur <= (tgt + tol));
        lv_obj_set_style_text_opa(ui.temp_label_target,
                                  in_range ? LV_OPA_60 : LV_OPA_COVER,
                                  LV_PART_MAIN);
    }

    // Triangle recolors
    lv_obj_set_style_img_recolor(ui.temp_tri_target, col_hex(UI_COLOR_TEMP_TARGET_HEX), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_target, LV_OPA_COVER, LV_PART_MAIN);

    {
        const uint32_t hex = temp_status_color_hex(state.tempCurrent, state.tempTarget);
        lv_obj_set_style_img_recolor(ui.temp_tri_current, col_hex(hex), LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(ui.temp_tri_current, LV_OPA_COVER, LV_PART_MAIN);
    }

    // Geometry mapping
    lv_coord_t scale_x = lv_obj_get_x(ui.temp_scale);
    lv_coord_t scale_y = lv_obj_get_y(ui.temp_scale);
    lv_coord_t scale_w = lv_obj_get_width(ui.temp_scale);
    lv_coord_t scale_h = lv_obj_get_height(ui.temp_scale);

    auto value_to_x = [&](int16_t value) -> lv_coord_t {
        float t = (float)(value - UI_TEMP_MIN_C) / (float)(UI_TEMP_MAX_C - UI_TEMP_MIN_C);
        if (t < 0.0f) {
            t = 0.0f;
        }
        if (t > 1.0f) {
            t = 1.0f;
        }
        return scale_x + (lv_coord_t)(t * (float)scale_w);
    };

    lv_coord_t tgt_x = value_to_x(tgt);
    lv_coord_t cur_x = value_to_x(cur);

    // Triangles centered on x
    lv_coord_t cur_tri_x = cur_x - (UI_TEMP_TRI_W / 2);
    lv_coord_t tgt_tri_x = tgt_x - (UI_TEMP_TRI_W / 2);

    // Y positions
    lv_coord_t cur_tri_y = scale_y - UI_TEMP_TRI_H - UI_TEMP_TRI_GAP_Y;
    lv_coord_t tgt_tri_y = scale_y + scale_h + UI_TEMP_TRI_GAP_Y;

    lv_obj_set_pos(ui.temp_tri_current, cur_tri_x, cur_tri_y);
    lv_obj_set_pos(ui.temp_tri_target, tgt_tri_x, tgt_tri_y);

    // Labels right of triangles
    lv_obj_set_pos(ui.temp_label_current,
                   cur_tri_x + UI_TEMP_TRI_W + UI_TEMP_LABEL_GAP_X,
                   cur_tri_y + (UI_TEMP_TRI_H / 2) - (lv_obj_get_height(ui.temp_label_current) / 2));

    lv_obj_set_pos(ui.temp_label_target,
                   tgt_tri_x + UI_TEMP_TRI_W + UI_TEMP_LABEL_GAP_X,
                   tgt_tri_y + (UI_TEMP_TRI_H / 2) - (lv_obj_get_height(ui.temp_label_target) / 2));
}

static void update_icons_and_rows(const OvenRuntimeState &st) {
    for (int i = 0; i < 7; ++i) {
        const bool on = get_port_on_by_index(st, i);

        // icons
        set_icon_onoff(ui.icon[i], on);

        // rows bg
        set_row_onoff(ui.row[i], on);

        // minimal content text (derived from runtime only)
        if (ui.row_label[i]) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s: %s", get_port_name_by_index(i), on ? "ON" : "OFF");
            lv_label_set_text(ui.row_label[i], buf);
        }
    }

    // Door special icon color: when door_open => red (overrides green)
    // (Only affects the icon; row stays green by "ON")
    if (ui.icon[4]) {
        if (st.door_open) {
            lv_obj_set_style_img_recolor(ui.icon[4], col_hex(UI_COLOR_ICON_DOOR_OPEN_HEX), LV_PART_MAIN);
            lv_obj_set_style_img_recolor_opa(ui.icon[4], LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_img_recolor(ui.icon[4], col_hex(UI_COLOR_ICON_DOOR_CLOSED_HEX), LV_PART_MAIN);
            lv_obj_set_style_img_recolor_opa(ui.icon[4], LV_OPA_COVER, LV_PART_MAIN);
        }
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

lv_obj_t *screen_dbg_hw_create(lv_obj_t *parent) {
    if (ui.root) {
        return ui.root;
    }

    // Geometry aligned with screen_main + base layout
    const int top_h = 50;
    const int page_h = UI_PAGE_INDICATOR_HEIGHT;
    const int bottom_h = 70;
    const int side_w = UI_SIDE_PADDING;
    const int center_w = UI_SCREEN_WIDTH - 2 * UI_SIDE_PADDING;

    screen_base_create(&ui.base, parent,
                       UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT,
                       top_h, page_h, bottom_h,
                       side_w, center_w);

    ui.root = ui.base.root;

    // Match gesture behavior
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui.root, LV_DIR_NONE);
    lv_obj_add_flag(ui.root, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // ------------------------------------------------------------------------
    // TOP: Link icon
    // ------------------------------------------------------------------------
    ui.icon_sync = lv_image_create(ui.base.top);
    lv_image_set_src(ui.icon_sync, &link_wht);
    lv_obj_set_size(ui.icon_sync, 32, 32);
    lv_obj_align(ui.icon_sync, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING - 50, 0);

    // ------------------------------------------------------------------------
    // LEFT: Icons container (like screen_main column)
    // ------------------------------------------------------------------------
    ui.icons_container = lv_obj_create(ui.base.left);
    lv_obj_clear_flag(ui.icons_container, LV_OBJ_FLAG_SCROLLABLE);
    //    lv_obj_remove_style_all(ui.icons_container);
    // lv_obj_set_size(ui.icons_container, LV_PCT(100),UI_DIAL_SIZE);
    lv_obj_set_size(ui.icons_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.icons_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.icons_container, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(ui.icons_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.icons_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Icon order (confirmed):
    // Fan12V, Fan230V, Fan230V_SLOW, Heater, Door, Motor, Lamp
    const void *srcs[7] = {
        &fan12v_wht,
        &fan230v_fast_wht,
        &fan230v_low_wht,
        &heater_wht,
        &door_open_wht,
        &motor230v,
        &lamp230v_wht,
    };

    for (int i = 0; i < 7; ++i) {
        ui.icon[i] = lv_image_create(ui.icons_container);
        lv_image_set_src(ui.icon[i], srcs[i]);
        lv_obj_set_size(ui.icon[i], 32, 32);

        // clickable (armed only when RUN gate is true)
        lv_obj_add_flag(ui.icon[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui.icon[i], icon_click_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // default OFF => white
        lv_obj_set_style_img_recolor_opa(ui.icon[i], LV_OPA_TRANSP, LV_PART_MAIN);

        // lv_obj_set_style_line_color(ui.lines[i], lv_color_hex(0xFF00FF), LV_PART_MAIN);
        // lv_obj_set_style_line_width(ui.lines[i], 4, LV_PART_MAIN);
    }
    // ------------------------------------------------------------------------
    // MIDDLE (CONTENT): 7 framed rows (minimal)
    // ------------------------------------------------------------------------
    ui.rows_container = lv_obj_create(ui.base.middle);
    // lv_obj_remove_style_all(ui.rows_container);
    lv_obj_set_size(ui.rows_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(ui.rows_container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(ui.rows_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(ui.rows_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.rows_container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui.rows_container, 6, 0);

    for (int i = 0; i < 7; ++i) {
        ui.row[i] = lv_obj_create(ui.rows_container);
        lv_obj_remove_style_all(ui.row[i]);

        // Size: full width, small fixed height
        lv_obj_set_width(ui.row[i], LV_PCT(100));
        lv_obj_set_height(ui.row[i], 35);

        // Frame: 1px border, 50% opacity
        lv_obj_set_style_border_width(ui.row[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.row[i], col_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_opa(ui.row[i], LV_OPA_50, LV_PART_MAIN);

        // Background: default (transparent) / green on update
        lv_obj_set_style_bg_opa(ui.row[i], LV_OPA_TRANSP, LV_PART_MAIN);

        // Padding inside
        lv_obj_set_style_pad_left(ui.row[i], 3, LV_PART_MAIN);
        lv_obj_set_style_pad_right(ui.row[i], 3, LV_PART_MAIN);
        lv_obj_set_style_pad_top(ui.row[i], 2, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(ui.row[i], 2, LV_PART_MAIN);

        // Label
        ui.row_label[i] = lv_label_create(ui.row[i]);
        lv_obj_set_style_text_color(ui.row_label[i], col_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_opa(ui.row_label[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_label_set_long_mode(ui.row_label[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ui.row_label[i], LV_PCT(100));
        lv_label_set_text(ui.row_label[i], "");
        lv_obj_align(ui.row_label[i], LV_ALIGN_LEFT_MID, 0, 0);

        // Make the whole row clickable (bigger hit target)
        lv_obj_add_flag(ui.row[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui.row[i], icon_click_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Let label clicks bubble up to the row (optional but nice)
        lv_obj_add_flag(ui.row_label[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ui.row_label[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // ------------------------------------------------------------------------
        // icon-row-container lines
        // ------------------------------------------------------------------------
        ui.lines[i] = lv_line_create(ui.root);
        lv_obj_remove_style_all(ui.lines[i]);
        lv_obj_add_flag(ui.lines[i], LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_pos(ui.lines[i], 0, 0);
        lv_obj_set_size(ui.lines[i], UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);

        lv_obj_set_style_line_width(ui.lines[i], 1, 0);
        lv_obj_set_style_line_rounded(ui.lines[i], false, 0);
        lv_obj_set_style_line_opa(ui.lines[i], LV_OPA_COVER, 0);
        lv_obj_set_style_line_color(ui.lines[i], lv_color_hex(UI_COLOR_ICON_OFF_HEX), LV_PART_MAIN);

        // initial dummy points
        ui.line_pts[i][0] = {0, 0};
        ui.line_pts[i][1] = {0, 0};
        ui.line_pts[i][2] = {0, 0};
        lv_line_set_points(ui.lines[i], ui.line_pts[i], 3);

        // DEBUG (optional)
        // lv_obj_set_style_line_color(ui.lines[i], lv_color_hex(0xFF00FF), LV_PART_MAIN);
        // lv_obj_set_style_line_width(ui.lines[i], 4, LV_PART_MAIN);
    }

    // ------------------------------------------------------------------------
    // RIGHT: RUN + CLEAR buttons (vertical)
    // ------------------------------------------------------------------------
    lv_obj_t *btn_col = lv_obj_create(ui.base.right);
    lv_obj_remove_style_all(btn_col);
    lv_obj_set_size(btn_col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(btn_col, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(btn_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(btn_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_col,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_col, 12, 0);

    // RUN button
    ui.btn_run = lv_btn_create(btn_col);
    lv_obj_set_size(ui.btn_run, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);
    lv_obj_set_style_radius(ui.btn_run, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.btn_run, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_add_event_cb(ui.btn_run, run_button_event_cb, LV_EVENT_CLICKED, nullptr);

    ui.lbl_run = lv_label_create(ui.btn_run);
    lv_label_set_text(ui.lbl_run, "RUN");
    lv_obj_center(ui.lbl_run);

    // CLEAR button
    // ui.btn_clear = lv_btn_create(btn_col);
    // lv_obj_set_size(ui.btn_clear, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);
    // lv_obj_set_style_radius(ui.btn_clear, 8, LV_PART_MAIN);
    // lv_obj_set_style_bg_grad_dir(ui.btn_clear, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    // lv_obj_set_style_bg_color(ui.btn_clear, col_hex(0x404040), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(ui.btn_clear, LV_OPA_COVER, LV_PART_MAIN);
    // lv_obj_add_event_cb(ui.btn_clear, clear_button_event_cb, LV_EVENT_CLICKED, nullptr);

    // ui.lbl_clear = lv_label_create(ui.btn_clear);
    // lv_label_set_text(ui.lbl_clear, "CLEAR");
    // lv_obj_center(ui.lbl_clear);

    // Initialize RUN gate UI
    g_run_gate = false;
    set_run_button_ui();

    // ------------------------------------------------------------------------
    // PAGE INDICATOR (panel + dots) + SWIPE HIT AREA (dedicated)
    // ------------------------------------------------------------------------
    ui.page_indicator_panel = lv_obj_create(ui.base.page_indicator);
    lv_obj_remove_style_all(ui.page_indicator_panel);
    lv_obj_set_size(ui.page_indicator_panel, 120, 24);
    lv_obj_center(ui.page_indicator_panel);

    lv_obj_set_style_radius(ui.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui.page_indicator_panel, col_hex(UI_COLOR_PANEL_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_panel, 4, 0);

    lv_obj_set_flex_flow(ui.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        ui.page_dots[i] = lv_obj_create(ui.page_indicator_panel);
        lv_obj_remove_style_all(ui.page_dots[i]);
        lv_obj_set_size(ui.page_dots[i], UI_PAGE_DOT_SIZE, UI_PAGE_DOT_SIZE);

        lv_obj_set_style_bg_opa(ui.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ui.page_dots[i], UI_PAGE_DOT_SIZE / 2, 0);
        lv_obj_set_style_border_opa(ui.page_dots[i], LV_OPA_TRANSP, 0);

        if (i != 0) {
            lv_obj_set_style_margin_left(ui.page_dots[i], UI_PAGE_DOT_SPACING, 0);
        }

        lv_color_t col = (i == 0)
                             ? col_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : col_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }

    // Swipe hit (wide, invisible-ish, clickable target)
    ui.swipe_hit = lv_obj_create(ui.base.page_indicator);
    lv_obj_remove_style_all(ui.swipe_hit);
    lv_obj_set_size(ui.swipe_hit, 360, UI_PAGE_INDICATOR_HEIGHT);
    lv_obj_align(ui.swipe_hit, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(ui.swipe_hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui.swipe_hit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui.swipe_hit, 15, 0);
    lv_obj_set_style_radius(ui.swipe_hit, 8, 0);

    ui.s_swipe_target = ui.swipe_hit;

    // ------------------------------------------------------------------------
    // BOTTOM: Temperature (minimal copy from screen_main)
    // ------------------------------------------------------------------------
    ui.temp_scale = lv_bar_create(ui.base.bottom);
    lv_obj_set_size(ui.temp_scale, UI_TEMP_SCALE_WIDTH, UI_TEMP_SCALE_HEIGHT);
    lv_obj_align(ui.temp_scale, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING, -6);
    lv_bar_set_range(ui.temp_scale, UI_TEMP_MIN_C, UI_TEMP_MAX_C);
    lv_bar_set_value(ui.temp_scale, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(ui.temp_scale, col_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.temp_scale, LV_OPA_COVER, LV_PART_MAIN);

    // Target triangle (up, below)
    ui.temp_tri_target = lv_image_create(ui.base.bottom);
    lv_image_set_src(ui.temp_tri_target, &temp_tri_up_wht);
    lv_obj_set_size(ui.temp_tri_target, UI_TEMP_TRI_W, UI_TEMP_TRI_H);
    lv_obj_add_flag(ui.temp_tri_target, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_target, LV_OPA_COVER, LV_PART_MAIN);

    ui.temp_label_target = lv_label_create(ui.base.bottom);
    lv_obj_set_style_text_color(ui.temp_label_target, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(ui.temp_label_target, "-- °C");
    lv_obj_add_flag(ui.temp_label_target, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // Current triangle (down, above)
    ui.temp_tri_current = lv_image_create(ui.base.bottom);
    lv_image_set_src(ui.temp_tri_current, &temp_tri_down_wht);
    lv_obj_set_size(ui.temp_tri_current, UI_TEMP_TRI_W, UI_TEMP_TRI_H);
    lv_obj_add_flag(ui.temp_tri_current, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_current, LV_OPA_COVER, LV_PART_MAIN);

    ui.temp_label_current = lv_label_create(ui.base.bottom);
    lv_obj_set_style_text_color(ui.temp_label_current, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(ui.temp_label_current, "-- °C");
    lv_obj_add_flag(ui.temp_label_current, LV_OBJ_FLAG_IGNORE_LAYOUT);

    UI_INFO("[screen_dbg_hw_create] root=%p swipe_target=%p\n",
            ui.root,
            ui.s_swipe_target);
    return ui.root;
}

void screen_dbg_hw_update_runtime(const OvenRuntimeState *state) {
    if (!state || !ui.root) {
        return;
    }

    if (state->linkSynced) {
        icon_link_synced(ui.icon_sync);
    } else {
        icon_link_unsynced(ui.icon_sync);
    }

    update_icons_and_rows(*state);
    update_temp_ui(*state);

    // --- NEW: ensure layout is up-to-date before using lv_obj_get_coords() ---
    lv_obj_update_layout(ui.root);

    // --- NEW: update all wires ---
    for (int i = 0; i < 7; ++i) {
        const bool on = get_port_on_by_index(*state, i);
        update_wire((uint8_t)i, ui.icon[i], ui.row[i], on);

        // optional: bring line to front if it is hidden behind panels
        // lv_obj_move_foreground(ui.lines[i]);
        lv_obj_move_background(ui.lines[i]);
    }
}

void screen_dbg_hw_disarm_and_clear(void) {
    // RUN gate off
    g_run_gate = false;
    set_run_button_ui();

    // clear content texts
    for (int i = 0; i < 7; ++i) {
        if (ui.row_label[i]) {
            lv_label_set_text(ui.row_label[i], "");
        }
    }

    // Optional (nur wenn du willst): Icons/Rows visuell zurücksetzen
    // (kommt später eh durch Telemetrie wieder korrekt)
    for (int i = 0; i < 7; ++i) {
        if (ui.icon[i]) {
            set_icon_onoff(ui.icon[i], false);
        }
        if (ui.row[i]) {
            set_row_onoff(ui.row[i], false);
        }
    }
}

void screen_dbg_hw_set_active_page(uint8_t page_index) {
    if (page_index >= UI_PAGE_COUNT) {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        lv_color_t col = (i == page_index)
                             ? col_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : col_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }
}

lv_obj_t *screen_dbg_hw_get_swipe_target(void) {

    return ui.s_swipe_target;
}

// END OF FILE