#pragma once

#include <lvgl.h>
#include "display/display_hsd040bpn1.h"
#include "touch.h"

// Globale UI-Objekte (optional, aber praktisch)
extern lv_obj_t *ui_ScreenMain;
extern lv_obj_t *ui_LabelInfo;
extern lv_obj_t *ui_ButtonTest;

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

// #pragma once

// #include <Arduino.h>
// #include <lvgl.h>
// #include <esp_heap_caps.h>

// #include "display_hsd040bpn1.h" // enthält: SCREEN_WIDTH/HEIGHT, gfx, init_display()
// #include "touch.h"              // Hersteller-Touch-Treiber (GT911 / TOUCH_GT911)

// typedef struct
// {
//     // touch-Display-Input-Device (Touch)
//     lv_indev_t *lv_touch_indev;
//     lv_display_t *gfxDriver = nullptr;
//     lv_obj_t *screen = nullptr;

//     // Widgets, Screens, etc. hier hinzufügen
//     lv_obj_t *btnStart = nullptr;

//     // Labels
//     lv_obj_t *lblBtnStart = nullptr;

// } UiControls;

// static UiControls *g_ui;

// void ui_init();
// void ui_task_handler();

// #include <lvgl.h>

// // UI initialisieren (Screens, Widgets, Events registrieren)
// void ui_init(void);