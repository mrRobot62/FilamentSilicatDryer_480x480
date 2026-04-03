#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_boot_create(lv_obj_t *parent);
lv_obj_t *screen_boot_get_swipe_target(void);

void screen_boot_set_progress(uint8_t percent);
void screen_boot_set_status(const char *text);

#ifdef __cplusplus
}
#endif
