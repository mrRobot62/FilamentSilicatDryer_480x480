#pragma once

#include <stdint.h>

void display_dimmer_init(void);
void display_dimmer_set_brightness_percent(uint8_t percent);
uint8_t display_dimmer_get_brightness_percent(void);
