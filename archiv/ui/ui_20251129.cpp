
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

    // // Aktuellen Screen holen (Standard-Screen)
    // ui_ScreenMain = lv_screen_active();

    // // Hintergrund
    // lv_obj_set_style_bg_color(ui_ScreenMain, lv_color_hex(0x202020), 0);
    // lv_obj_set_style_bg_opa(ui_ScreenMain, LV_OPA_COVER, 0);

    // // Label oben
    // ui_LabelInfo = lv_label_create(ui_ScreenMain);
    // lv_label_set_text(ui_LabelInfo, "Touch me!");
    // lv_obj_set_style_text_color(ui_LabelInfo, lv_color_hex(0xFFFFFF), 0);
    // lv_obj_align(ui_LabelInfo, LV_ALIGN_TOP_MID, 0, 20);

    // // Button in der Mitte
    // ui_ButtonTest = lv_button_create(ui_ScreenMain);
    // lv_obj_set_size(ui_ButtonTest, 200, 80);
    // lv_obj_center(ui_ButtonTest);

    // lv_obj_t *btn_label = lv_label_create(ui_ButtonTest);
    // lv_label_set_text(btn_label, "Button");
    // lv_obj_center(btn_label);

    // // Event-Callback registrieren
    // lv_obj_add_event_cb(ui_ButtonTest, ui_event_ButtonTest, LV_EVENT_CLICKED, nullptr);
}

// #include "ui.h"

// // LVGL Display handle
// static lv_display_t *lv_disp = nullptr;

// // LVGL draw buffer + Speicher
// static lv_draw_buf_t lv_draw_buf;
// static lv_color_t *lv_buf1 = nullptr;

// // Optional: LVGL Input-Device (Touch)
// static lv_indev_t *lv_touch_indev = nullptr;

// /*-------------------------
//  * LVGL Flush-Callback
//  *------------------------*/
// static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
// {
//     uint32_t w = (area->x2 - area->x1 + 1);
//     uint32_t h = (area->y2 - area->y1 + 1);

//     // LV_COLOR_DEPTH = 16 -> px_map ist RGB565
//     uint16_t *src = (uint16_t *)px_map;

//     gfx->draw16bitRGBBitmap(area->x1, area->y1, src, w, h);

//     lv_display_flush_ready(disp);
// }

// /*-------------------------
//  * LVGL Touch-Read-Callback
//  *------------------------*/
// static void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
// {
//     static bool last_pressed = false;
//     bool pressed = false;

//     // GT911-Zweig in touch.h:
//     //  - touch_has_signal() gibt bei GT911 immer true zurück
//     //  - touch_touched() ruft ts.read() auf und setzt touch_last_x / touch_last_y
//     Serial.println(F("[TOUCH] 1 my_touch_read() called"));
//     if (touch_has_signal())
//     {
//         Serial.println(F("[TOUCH] 2 touch_has_signal() called"));
//         if (touch_touched())
//         {
//             Serial.println(F("[TOUCH] 3 touch_touched() called"));
//             pressed = true;
//             data->point.x = touch_last_x;
//             data->point.y = touch_last_y;
//         }
//         else if (touch_released())
//         {
//             pressed = false;
//         }
//     }
//     else
//     {
//         pressed = false;
//     }

//     data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

//     // Debug: nur bei neuem Press-Event loggen
//     if (pressed && !last_pressed)
//     {
//         Serial.print(F("[TOUCH] PRESSED at "));
//         Serial.print(data->point.x);
//         Serial.print(F(", "));
//         Serial.println(data->point.y);
//     }

//     if (!pressed && last_pressed)
//     {
//         Serial.println(F("[TOUCH] RELEASED"));
//     }

//     last_pressed = pressed;
// }

// /*-------------------------
//  * LVGL Test-UI (Button)
//  *------------------------*/
// static void btn_event_cb(lv_event_t *e)
// {
//     if (lv_event_get_code(e) == LV_EVENT_CLICKED)
//     {
//         Serial.println(F("[LVGL] Button CLICKED"));
//     }
// }

// static void lv_create_test_screen()
// {
//     g_ui.screen = lv_screen_active();

//     // Hintergrund
//     lv_obj_set_style_bg_color(g_ui.screen, lv_color_hex(0x202020), 0);
//     lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);

//     // Button in der Mitte
//     g_ui.btnStart = lv_button_create(g_ui.screen);
//     lv_obj_set_size(g_ui.btnStart, 160, 80);
//     lv_obj_center(g_ui.btnStart);

//     lv_obj_t *label = lv_label_create(g_ui.btnStart);
//     lv_label_set_text(label, "Touch me");
//     lv_obj_center(label);

//     lv_obj_add_event_cb(g_ui.btnStart, btn_event_cb, LV_EVENT_CLICKED, nullptr);
// }

// void ui_init()
// {
//     Serial.println(F("[UI] ui_init() called"));

//     // 1) Display initialisieren (in main.cpp)

//     // 2) LVGL initialisieren
//     lv_init();
//     Serial.println(F("[UI] lv_init() OK"));

//     // 3) Draw-Buffer anlegen
//     const uint32_t buf_lines = 80;
//     const uint32_t buf_pixels = SCREEN_WIDTH * buf_lines;

//     lv_buf1 = (lv_color_t *)heap_caps_malloc(
//         buf_pixels * sizeof(lv_color_t),
//         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

//     if (!lv_buf1)
//     {
//         Serial.println(F("[UI] LVGL draw buffer alloc FAILED"));
//         while (true)
//         {
//             delay(1000);
//         }
//     }
//     Serial.print(F("[UI] LVGL draw buffer alloc OK, pixels="));
//     Serial.println(buf_pixels);

//     // 4) Display-Treiber für LVGL anlegen
//     g_ui.gfxDriver = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
//     if (!g_ui.gfxDriver)
//     {
//         Serial.println(F("[UI] lv_display_create() FAILED"));
//         while (true)
//         {
//             delay(1000);
//         }
//     }
//     Serial.println(F("[UI] lv_display_create() OK"));

//     // Flush-Callback setzen
//     lv_display_set_flush_cb(lv_disp, my_disp_flush);

//     // Draw-Buffer verbinden
//     lv_display_set_draw_buffers(
//         lv_disp,
//         &lv_draw_buf,
//         nullptr);

//     Serial.println(F("[UI] lv_display_set_draw_buffers() OK"));

//     // 5) Input-Device (Touch) anlegen
//     lv_touch_indev = lv_indev_create();
//     lv_indev_set_type(lv_touch_indev, LV_INDEV_TYPE_POINTER);
//     lv_indev_set_read_cb(lv_touch_indev, my_touch_read);
//     lv_indev_set_display(lv_touch_indev, lv_disp); // auf dieses Display mappen

//     Serial.println(F("[UI] lv_indev (touch) created"));

//     // 6) Test-Screen erzeugen
//     lv_create_test_screen();
//     Serial.println(F("[UI] lv_create_test_screen() OK"));
// }

// void ui_task_handler()
// {
//     lv_timer_handler();
// }