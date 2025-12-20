#include "nav.h"
#include "screen_manager.h"

void nav_next(void)
{
    ScreenId cur = screen_manager_current();
    ScreenId next = (ScreenId)((cur + 1) % SCREEN_COUNT);
    screen_manager_show(next);
}

void nav_prev(void)
{
    ScreenId cur = screen_manager_current();
    ScreenId prev = (ScreenId)((cur + SCREEN_COUNT - 1) % SCREEN_COUNT);
    screen_manager_show(prev);
}

void nav_go(ScreenId id)
{
    screen_manager_show(id);
}