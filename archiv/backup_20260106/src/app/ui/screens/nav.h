#pragma once
#include <stdbool.h>
#include "screen_manager.h" // <-- reuse ScreenId

#ifdef __cplusplus
extern "C"
{
#endif

    void nav_next(void);
    void nav_prev(void);
    void nav_go(ScreenId id);

#ifdef __cplusplus
}
#endif