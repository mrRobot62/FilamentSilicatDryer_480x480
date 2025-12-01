#pragma once

#include <lvgl.h>

// -------- MAIN SCREEN EVENTS --------

// Start/Stop button on the main screen
void ui_event_main_start_button(lv_event_t *e);

// Navigation buttons on main screen
void ui_event_main_open_config(lv_event_t *e);
void ui_event_main_open_log(lv_event_t *e);

// Icon buttons with manual override (if allowed by oven state)
void ui_event_main_fan230_clicked(lv_event_t *e);
void ui_event_main_motor_clicked(lv_event_t *e);
void ui_event_main_lamp_clicked(lv_event_t *e);

// -------- CONFIG SCREEN EVENTS --------

// "Apply/Save" button: read profile from config screen, send to oven, go back to main
void ui_event_config_apply(lv_event_t *e);

// "Cancel/Back" button: discard changes and go back to main
void ui_event_config_cancel(lv_event_t *e);

// -------- LOG SCREEN EVENTS --------

// "Clear log" button: clear core log buffer and refresh log screen
void ui_event_log_clear(lv_event_t *e);

// "Back" button: return to main screen
void ui_event_log_back(lv_event_t *e);