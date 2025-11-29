#pragma once

#include <lvgl.h>
#include "display/display_hsd040bpn1.h"
#include "touch.h"

// Globale UI-Objekte (optional, aber praktisch)
//extern lv_obj_t *ui_screen;
// extern lv_obj_t *ui_LabelInfo;
// extern lv_obj_t *ui_ButtonTest;

typedef struct
{
    // Display-Treiber für LVGL
    lv_display_t *displayDrv = nullptr;
    lv_draw_buf_t lv_draw_buf;
    lv_color_t *lv_buf1 = nullptr;

    // touch-Display-Input-Device (Touch)
    lv_indev_t *lv_touch_indev;
    lv_obj_t *screen = nullptr;

    // Widgets, Screens, etc. hier hinzufügen
    lv_obj_t *btnStart = nullptr;

    // Labels
    lv_obj_t *lblBtnStart = nullptr;

} UiControls;

static UiControls g_ui;

// UI initialisieren (Screens, Widgets, Events registrieren)
void ui_init(void);
