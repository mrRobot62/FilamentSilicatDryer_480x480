
#include "ui/ui.h"
#include "ui/ui_events.h"

// Definition der globalen UI-Objekt-Pointer
lv_obj_t *ui_ScreenMain = nullptr;
lv_obj_t *ui_LabelInfo = nullptr;
lv_obj_t *ui_ButtonTest = nullptr;

// Flush-Callback: LVGL -> GFX
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // LV_COLOR_DEPTH = 16 -> px_map zeigt auf RGB565
    uint16_t *src = (uint16_t *)px_map;

    gfx->draw16bitRGBBitmap(area->x1, area->y1, src, w, h);

    lv_display_flush_ready(disp);
}

// Touch-Callback für LVGL 9
static void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);

    static uint32_t read_counter = 0;
    read_counter++;

    // Alle 50 Aufrufe eine Debug-Zeile ausgeben
    if (read_counter % 50 == 0)
    {
        Serial.print(F("[LVGL TOUCH] read_cb called, count="));
        Serial.println(read_counter);
    }

    bool touched = false;

    if (touch_has_signal())
    {
        touched = touch_touched();
    }

    if (touched)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;

        Serial.print(F("[TOUCH] PRESSED at "));
        Serial.print(data->point.x);
        Serial.print(F(", "));
        Serial.println(data->point.y);
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void ui_build()
{
    // Aktuellen Screen holen (Standard-Screen)
    ui_ScreenMain = lv_screen_active();

    // Hintergrund
    lv_obj_set_style_bg_color(ui_ScreenMain, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(ui_ScreenMain, LV_OPA_COVER, 0);

    // Label oben
    ui_LabelInfo = lv_label_create(ui_ScreenMain);
    lv_label_set_text(ui_LabelInfo, "Touch me!");
    lv_obj_set_style_text_color(ui_LabelInfo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(ui_LabelInfo, LV_ALIGN_TOP_MID, 0, 20);

    // Button in der Mitte
    ui_ButtonTest = lv_button_create(ui_ScreenMain);
    lv_obj_set_size(ui_ButtonTest, 200, 80);
    lv_obj_center(ui_ButtonTest);

    lv_obj_t *btn_label = lv_label_create(ui_ButtonTest);
    lv_label_set_text(btn_label, "Button");
    lv_obj_center(btn_label);

    // Event-Callback registrieren
    lv_obj_add_event_cb(ui_ButtonTest, ui_event_ButtonTest, LV_EVENT_CLICKED, nullptr);
}

/**
 * -------------------------
 * UI_INIT
 * -------------------------
 * Display & Touch initialisieren und anschließend UI aufbauen
 * ui_build() : Initialisiert LVGL, Display, Touch und baut die UI auf.
 *
 */
void ui_init(void)
{

    // 1) Display initialisieren (in main.cpp)
    if (!init_display())
    {
        {
            Serial.println(F("[UI] init_display() FAILED"));
            while (true)
            {
                delay(1000);
            }
        }
    }
    Serial.println(F("[UI] (1) init_display() OK"));

    // 2) LVGL initialisieren
    lv_init();
    Serial.println(F("[UI] (2) lv_init() OK"));

    const uint32_t buf_lines = 80;
    const uint32_t buf_pixels = SCREEN_WIDTH * buf_lines;

    Serial.println(F("[UI] Allocating LVGL draw buffer..."));
    g_ui.lv_buf1 = (lv_color_t *)heap_caps_malloc(
        buf_pixels * sizeof(lv_color_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    Serial.println(F("[UI] Allocating LVGL draw buffer done"));

    if (!g_ui.lv_buf1)
    {
        Serial.println(F("[UI] LVGL draw buffer alloc FAILED"));
        while (true)
        {
            delay(1000);
        }
    }
    Serial.print(F("[UI] (3) LVGL draw buffer alloc OK, pixels="));
    Serial.println(buf_pixels);

    lv_draw_buf_init(
        &g_ui.lv_draw_buf,
        SCREEN_WIDTH,
        buf_lines,
        LV_COLOR_FORMAT_RGB565,
        SCREEN_WIDTH,
        g_ui.lv_buf1,
        buf_pixels * sizeof(lv_color_t));

    // 4) Display-Objekt
    g_ui.displayDrv = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!g_ui.displayDrv)
    {
        Serial.println(F("[UI] lv_display_create() FAILED"));
        while (true)
        {
            delay(1000);
        }
    }
    Serial.println(F("[UI] lv_display_create() OK"));

    lv_display_set_flush_cb(g_ui.displayDrv, my_disp_flush);
    lv_display_set_draw_buffers(g_ui.displayDrv, &g_ui.lv_draw_buf, nullptr);
    Serial.println(F("[UI] lv_display_set_draw_buffers() OK"));

    // 5) Touch initialisieren
    touch_init();
    Serial.println(F("[UI] touch_init() OK"));

    // 6) LVGL Input-Device einrichten
    g_ui.lv_touch_indev = lv_indev_create();
    lv_indev_set_type(g_ui.lv_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_ui.lv_touch_indev, my_touch_read);
    lv_indev_set_display(g_ui.lv_touch_indev, g_ui.displayDrv); // auf dieses Display mappen
    Serial.println(F("[UI] LVGL input device (touch) created"));

    // 7) UI erzeugen
    ui_build();
    Serial.println(F("[UI] ui_build_ui() OK"));
}
