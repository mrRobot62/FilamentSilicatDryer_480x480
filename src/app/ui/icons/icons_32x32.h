#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** 32x32 icons (all white images, recolorable at runtime) */

    extern const lv_image_dsc_t fan12v_wht;
    extern const lv_image_dsc_t fan230v_low_wht;
    extern const lv_image_dsc_t fan230v_fast_wht;
    extern const lv_image_dsc_t heater_wht;
    extern const lv_image_dsc_t lamp230v_wht;
    extern const lv_image_dsc_t motor230v;
    extern const lv_image_dsc_t door_open_wht;
    extern const lv_image_dsc_t temp_tri_up_wht;
    extern const lv_image_dsc_t temp_tri_down_wht;

#ifdef __cplusplus
} /* extern "C" */
#endif