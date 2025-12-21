#include "screen_manager.h"

#include "screen_main.h"
#include "screen_config.h"
#include "screen_log.h"

#include "oven/oven.h" // bool oven_is_running()

#include <cstdlib> // abs()

/* ============================================================================
 * Globals
 * ==========================================================================*/

static lv_obj_t *s_parent = nullptr;   // usually lv_scr_act()
static lv_obj_t *s_app_root = nullptr; // app root container

static lv_obj_t *s_screens[SCREEN_COUNT] = {nullptr};
static ScreenId s_current = SCREEN_MAIN;

/* ============================================================================
 * Swipe detection state
 * ==========================================================================*/

static lv_point_t s_swipe_start;
static bool s_swipe_active = false;
static bool s_swipe_fired = false;

// Tuned for 480x480
static constexpr int kSwipeMinDxPx = 60;
static constexpr int kSwipeMaxDyPx = 40;

/* ============================================================================
 * Helpers
 * ==========================================================================*/

static ScreenId next_id(ScreenId id)
{
    const bool running = oven_is_running();

    if (running)
    {
        // While running: only MAIN <-> LOG
        if (id == SCREEN_MAIN)
            return SCREEN_LOG;
        if (id == SCREEN_LOG)
            return SCREEN_LOG; // clamp at LOG
        // if somehow in CONFIG while running, treat as MAIN -> LOG
        return SCREEN_LOG;
    }

    // Not running: MAIN -> CONFIG -> LOG (clamped)
    const int last = SCREEN_COUNT - 1;
    if ((int)id >= last)
        return (ScreenId)last;
    return (ScreenId)((int)id + 1);
}

static ScreenId prev_id(ScreenId id)
{
    const bool running = oven_is_running();

    if (running)
    {
        // While running: only MAIN <-> LOG
        if (id == SCREEN_LOG)
            return SCREEN_MAIN;
        if (id == SCREEN_MAIN)
            return SCREEN_MAIN; // clamp at MAIN
        // if somehow in CONFIG while running, go back to MAIN
        return SCREEN_MAIN;
    }

    // Not running: LOG -> CONFIG -> MAIN (clamped)
    if ((int)id <= 0)
        return (ScreenId)0;
    return (ScreenId)((int)id - 1);
}

static bool navigation_allowed(void)
{
    // Navigation disabled while oven is running
    return !oven_is_running();
}

/* ============================================================================
 * Swipe callback (PRESSED + PRESSING based)
 * ==========================================================================*/

static void app_swipe_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    // if (!navigation_allowed())
    //     return;

    lv_indev_t *indev = lv_indev_active();
    if (!indev)
        return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED)
    {
        s_swipe_start = p;
        s_swipe_active = true;
        s_swipe_fired = false;
        return;
    }

    if (code == LV_EVENT_PRESSING)
    {
        if (!s_swipe_active || s_swipe_fired)
            return;

        int dx = p.x - s_swipe_start.x;
        int dy = p.y - s_swipe_start.y;

        if (abs(dy) > kSwipeMaxDyPx)
            return;

        if (abs(dx) < kSwipeMinDxPx)
            return;

        s_swipe_fired = true;

        if (dx < 0)
        {
            UI_INFO("[ScreenManager] SWIPE LEFT\n");
            screen_manager_show(next_id(s_current));
        }
        else
        {
            UI_INFO("[ScreenManager] SWIPE RIGHT\n");
            screen_manager_show(prev_id(s_current));
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
        s_swipe_active = false;
        return;
    }
}

/* ============================================================================
 * Attach swipe handler to a dedicated swipe target
 * ==========================================================================*/

static void attach_swipe_target(lv_obj_t *obj)
{
    if (!obj)
        return;

    // Needs to receive pointer events
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(obj, app_swipe_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, app_swipe_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, app_swipe_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, app_swipe_cb, LV_EVENT_PRESS_LOST, NULL);
}

/* ============================================================================
 * Public API
 * ==========================================================================*/

lv_obj_t *screen_manager_app_root(void)
{
    return s_app_root;
}

ScreenId screen_manager_current(void)
{
    return s_current;
}

void screen_manager_show(ScreenId id)
{
    UI_INFO("[ScreenManager] show(%d) cur=%d ptr=%p\n",
            (int)id, (int)s_current, (void *)s_screens[id]);
    if (!s_app_root)
        return;
    if (id < 0 || id >= SCREEN_COUNT)
        return;
    if (!s_screens[id])
        return;

    for (int i = 0; i < SCREEN_COUNT; ++i)
    {
        if (s_screens[i])
            lv_obj_add_flag(s_screens[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clear_flag(s_screens[id], LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screens[id]);

    s_current = id;

    UI_INFO("[ScreenManager] screens: main=%p cfg=%p log=%p current:%p\n",
            (void *)s_screens[SCREEN_MAIN],
            (void *)s_screens[SCREEN_CONFIG],
            (void *)s_screens[SCREEN_LOG]);
}

void screen_manager_init(lv_obj_t *screen_parent)
{
    s_parent = screen_parent ? screen_parent : lv_scr_act();

    /* App root */
    s_app_root = lv_obj_create(s_parent);
    lv_obj_remove_style_all(s_app_root);
    lv_obj_set_size(s_app_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_app_root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_app_root, lv_color_black(), 0);

    /* Create screens */
    s_screens[SCREEN_MAIN] = screen_main_create(s_app_root);
    s_screens[SCREEN_CONFIG] = screen_config_create(s_app_root);
    s_screens[SCREEN_LOG] = screen_log_create(s_app_root);

    /* Hide all initially */
    for (int i = 0; i < SCREEN_COUNT; ++i)
    {
        if (s_screens[i])
            lv_obj_add_flag(s_screens[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Attach swipe only to dedicated swipe zones */
    attach_swipe_target(screen_main_get_swipe_target());
    attach_swipe_target(screen_config_get_swipe_target());
    attach_swipe_target(screen_log_get_swipe_target());

    /* Default screen after boot */
    screen_manager_show(SCREEN_MAIN);
    UI_DBG("[SCR_MANAGER] done. screen-addr: %d\n", s_app_root);
}

void screen_manager_go_home(void)
{
    screen_manager_show(SCREEN_MAIN);
}
