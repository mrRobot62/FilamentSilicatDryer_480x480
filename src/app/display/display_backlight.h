#pragma once

#include <stdint.h>

void display_backlight_init(void);
void display_backlight_set_percent(uint8_t percent);
uint8_t display_backlight_get_percent(void);
