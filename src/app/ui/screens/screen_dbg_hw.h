#pragma once
#include "oven.h" // OvenRuntimeState
#include "screen_base.h"
#include <lvgl.h>

static constexpr uint8_t HW_ROWS = 7;

lv_obj_t *screen_dbg_hw_create(lv_obj_t *parent);
void screen_dbg_hw_update_runtime(const OvenRuntimeState *state);
void screen_dbg_hw_set_active_page(uint8_t page_index);
lv_obj_t *screen_dbg_hw_get_swipe_target(void);
void screen_dbg_hw_disarm_and_clear(void);

// END OF FILE
