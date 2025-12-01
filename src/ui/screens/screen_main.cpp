#include "screen_main.h"
#include "../ui_events.h"

// Static pointers to widgets on the main screen.
// They are only used inside this file.
static lv_obj_t *screen_main_root = nullptr;
static lv_obj_t *label_time = nullptr;
static lv_obj_t *label_temp = nullptr;
static lv_obj_t *label_filament = nullptr;
static lv_obj_t *button_start = nullptr;
static lv_obj_t *button_start_label = nullptr;

// Helper function to format seconds as "HH:MM:SS"
static void format_time_hh_mm_ss(uint32_t seconds, char *buffer, size_t bufferSize)
{
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t sec = seconds % 60;

    // Example: "01:23:45"
    snprintf(buffer, bufferSize, "%02u:%02u:%02u",
             (unsigned int)hours,
             (unsigned int)minutes,
             (unsigned int)sec);
}

lv_obj_t *screen_main_create(void)
{
    // If the screen was already created, just return it.
    if (screen_main_root != nullptr)
    {
        return screen_main_root;
    }

    // Create a new root object without a parent -> becomes a screen
    screen_main_root = lv_obj_create(nullptr);

    // Basic layout: vertical column, centered
    lv_obj_set_size(screen_main_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(screen_main_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        screen_main_root,
        LV_FLEX_ALIGN_START,  // main axis
        LV_FLEX_ALIGN_CENTER, // cross axis
        LV_FLEX_ALIGN_CENTER  // track alignment
    );
    lv_obj_set_style_pad_all(screen_main_root, 12, 0);
    lv_obj_set_style_pad_row(screen_main_root, 8, 0);

    // --- Time label (countdown) ---
    label_time = lv_label_create(screen_main_root);
    lv_label_set_text(label_time, "Time: 00:00:00");

    // --- Temperature label (current / target) ---
    label_temp = lv_label_create(screen_main_root);
    lv_label_set_text(label_temp, "Temp: --.- / --.- \xC2\xB0"
                                  "C");

    // --- Filament label ---
    label_filament = lv_label_create(screen_main_root);
    lv_label_set_text(label_filament, "Filament: (none)");

    // --- Start button ---
    button_start = lv_button_create(screen_main_root);

    button_start_label = lv_label_create(button_start);
    lv_label_set_text(button_start_label, "Start");
    lv_obj_center(button_start_label);

    // Optional: Anfangsfarbe des Buttons
    lv_obj_set_style_bg_color(button_start,
                              lv_color_hex(0x44CC44),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // Attach global event handler (implemented in ui_events.cpp)
    lv_obj_add_event_cb(button_start, ui_event_main_start_button, LV_EVENT_CLICKED, nullptr);

    return screen_main_root;
}

void screen_main_update_runtime(const OvenRuntimeState *state)
{
    if (screen_main_root == nullptr || state == nullptr)
    {
        return;
    }

    // --- Update time label (countdown) ---
    if (label_time != nullptr)
    {
        char timeBuf[32];
        format_time_hh_mm_ss(state->secondsRemaining, timeBuf, sizeof(timeBuf));

        char buf[48];
        // Example: "Time: 01:23:45"
        snprintf(buf, sizeof(buf), "Time: %s", timeBuf);
        lv_label_set_text(label_time, buf);
    }

    // --- Update temperature label (current / target) ---
    if (label_temp != nullptr)
    {
        char buf[64];
        // Example: "Temp: 25.0 / 70.0 °C"
        snprintf(
            buf,
            sizeof(buf),
            "Temp: %.1f / %.1f \xC2\xB0"
            "C",
            (double)state->tempCurrent,
            (double)state->tempTarget);
        lv_label_set_text(label_temp, buf);
    }

    // --- Update filament label ---
    if (label_filament != nullptr)
    {
        char buf[64];
        // For now we only show the filamentId.
        // Later we can map this to a preset name.
        snprintf(buf, sizeof(buf), "Filament: #%d", state->filamentId);
        lv_label_set_text(label_filament, buf);
    }

    // --- Update start button label and style based on running state ---
    if (button_start != nullptr && button_start_label != nullptr)
    {
        if (state->running)
        {
            lv_label_set_text(button_start_label, "Stop");

            lv_obj_set_style_bg_color(button_start,
                                      lv_color_hex(0xFF4444),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            lv_label_set_text(button_start_label, "Start");

            lv_obj_set_style_bg_color(button_start,
                                      lv_color_hex(0x44CC44),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // --- TODO: später Icons aktualisieren (fan, heater, door, motor, lamp) ---
}