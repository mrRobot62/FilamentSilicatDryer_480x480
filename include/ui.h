#pragma once

#include <lvgl.h>
#include "display/display_hsd040bpn1.h"
#include "touch.h"

// UI Scale TIME
#define SCALE_TIME_HOUR_START 0
#define SCALE_TIME_HOUR_END 12

// zentrale STRUCT
// nimmt lediglich die Screens auf, das Display und Touch
// keine Widgets, die sind jeweils in den entsprechenden Screens verankert
typedef struct
{
    lv_display_t *displayDrv = nullptr;
    lv_draw_buf_t lv_draw_buf;
    lv_color_t *lv_buf1 = nullptr;

    lv_indev_t *lv_touch_indev = nullptr;

    lv_obj_t *screenMain = nullptr;
    lv_obj_t *screenConfig = nullptr;
    lv_obj_t *screenLog = nullptr;

} UiControls;

extern UiControls g_ui;

// UI initialisieren (Screens, Widgets, Events registrieren)
void ui_init(void);
