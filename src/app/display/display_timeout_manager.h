#pragma once

#include <stdint.h>

#include "oven.h"

void display_timeout_init(void);
void display_timeout_reload_from_host_parameters(void);
bool display_timeout_consume_wake_touch(void);
void display_timeout_note_user_activity(void);
void display_timeout_note_runtime_state(const OvenRuntimeState *state);
void display_timeout_tick(uint32_t now_ms);
