#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#include "icons_32x32.h"

/*
 * IMPORTANT:
 *  - Do NOT compile the following *.c files separately.
 *  - They are included here as implementation.
 *  - Either keep them in the same folder or adjust include paths.
 */

#include "../assets/icons/motor230v.c"        // ELECTRIC_MOTOR, fan_2
#include "../assets/icons/fan12v_wht.c"       // fan12v_wht
#include "../assets/icons/fan230v_low_wht.c"  // fan230v_low_wht
#include "../assets/icons/fan230v_fast_wht.c" // fan230v_fast_wht
#include "../assets/icons/heater_wht.c"       // heater_wht
#include "../assets/icons/lamp230v_wht.c"     // lamp230v_wht
#include "../assets/icons/door_open_wht.c"     // lamp230v_wht
#include "../assets/icons/temp_tri_up_wht.c"     // Hour needle_wht
#include "../assets/icons/temp_tri_down_wht.c"     // Hour needle_wht
#include "../assets/icons/link_16x16.c"               // Host/Client Sync icon

