#pragma once
#include "ui.h"
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------------
// screen order
// please note to include a new screen
// update UI_PAGE_COUNT too
//-------------------------------
typedef enum ScreenId {
    SCREEN_MAIN = 0,
    SCREEN_CONFIG,
    SCREEN_DBG_HW,
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

void screen_manager_go_home(void);
static void handle_swipe_safety_before_leave(void);

#ifdef __cplusplus
} // extern "C"
#endif

// END OF FILE
