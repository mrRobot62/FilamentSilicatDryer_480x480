#pragma once

#include <lvgl.h>
#include "oven/oven.h"

// Create the main screen and return its root object.
// This does NOT load the screen, it only creates it.
// ui_show_screen_main() will later call lv_screen_load() on this object.
lv_obj_t *screen_main_create(void);

// Update all runtime-related widgets (time scale, temperature scales,
// filament label, icon colors, etc.) based on the current oven state.
void screen_main_update_runtime(const OvenRuntimeState *state);