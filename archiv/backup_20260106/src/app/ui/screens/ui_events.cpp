#include "ui_events.h"
#include "ui.h"
#include "oven/oven.h"
#include "log_events.h" // falls du Logging schon nutzt, sonst erstmal weglassen

// MAIN - Door open/close
void ui_event_main_open_config(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_main_open_config (not implemented) code=%d", (int)code);
}

// MAIN - FAN230 on/off
void ui_event_main_fan230_clicked(lv_event_t *e)
{

    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_main_fan230_clicked (not implemented) code=%d", (int)code);
}

// MAIN - Silicat-Motor
void ui_event_main_motor_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_main_motor_clicked (not implemented) code=%d", (int)code);
}

// MAIN - LAMP
void ui_event_main_lamp_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_main_lamp_clicked (not implemented) code=%d", (int)code);
}

// -------- CONFIG SCREEN EVENTS --------
void ui_event_config_apply(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_config_apply (not implemented) code=%d", (int)code);
}

void ui_event_config_cancel(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_config_cancel (not implemented) code=%d", (int)code);
}

// -------- LOG SCREEN EVENTS --------
void ui_event_log_clear(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_log_clear (not implemented) code=%d", (int)code);
}

void ui_event_log_back(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);
    EVENT_INFO("ui_event_log_back (not implemented) code=%d", (int)code);
}