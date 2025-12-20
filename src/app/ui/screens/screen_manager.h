#pragma once
#include <lvgl.h>
#include "ui.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ScreenId
    {
        SCREEN_MAIN = 0,
        SCREEN_CONFIG,
        SCREEN_LOG,
        SCREEN_COUNT
    } ScreenId;

    void screen_manager_init(lv_obj_t *screen_parent); // typically lv_scr_act()
    // create the root container for screens
    lv_obj_t *screen_manager_app_root(void);

    // switch to the specified screen
    void screen_manager_show(ScreenId id);

    // get the currently active screen
    ScreenId screen_manager_current(void);

#ifdef __cplusplus
} // extern "C"
#endif