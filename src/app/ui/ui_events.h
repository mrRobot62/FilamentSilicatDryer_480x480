#pragma once

#include <lvgl.h>

// -------- MAIN SCREEN EVENTS -----------------------------------------------------
// ui_events.h/cpp Eventsteuerung für alle Widgets egal auf welchem Bildschirm
// ---------------------------------------------------------------------------------

// Start/Stop button on the main screen
void ui_event_main_start_button(lv_event_t *e);

// Navigation buttons on main scree;
void ui_event_main_open_log(lv_event_t *e);

// Icon buttons with manual override (if allowed by oven state)
void ui_event_main_fan230_clicked(lv_event_t *e);
void ui_event_main_motor_clicked(lv_event_t *e);
void ui_event_main_lamp_clicked(lv_event_t *e);

// -------- CONFIG SCREEN EVENTS --------

// "SAVE-Button", liest Widgets aus und sendet Daten zum Ofen, dann zurück zum Main-Screen
void ui_event_config_apply(lv_event_t *e);

// "CANCAL/BACK" button: bricht aktuelle Konfiguration ab, kehrt zum Main-Screen zurück
void ui_event_config_cancel(lv_event_t *e);

// -------- LOG SCREEN EVENTS --------

// "CLEAR" Button: löscht alle Logeinträge aus dem Buffer, refresh des Screens
void ui_event_log_clear(lv_event_t *e);

// "Back" button: zurück zum Main-Screen
void ui_event_log_back(lv_event_t *e);