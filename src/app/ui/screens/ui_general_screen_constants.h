// Forward-declare your OvenRuntimeState.
// Adjust the include if you already have a header for this.
// struct OvenRuntimeState;

// Layout constants (geometry)
#define UI_SCREEN_WIDTH 480
#define UI_SCREEN_HEIGHT 480

#define UI_SIDE_PADDING 60
#define UI_TOP_PADDING 5
#define UI_BOTTOM_PADDING 5
#define UI_FRAME_PADDING 10

#define UI_DIAL_SIZE 300
#define UI_DIAL_MIN_TICKS 5

#define UI_TIME_BAR_WIDTH 360 // 480 - 2 * 60
#define UI_TIME_BAR_HEIGHT 14

#define UI_TEMP_SCALE_WIDTH 360
#define UI_TEMP_SCALE_HEIGHT 15
#define UI_TEMP_MIN_C 0
#define UI_TEMP_MAX_C 120

#define UI_START_BUTTON_SIZE 100

#define UI_PAGE_COUNT 4
#define UI_PAGE_INDICATOR_HEIGHT 40
#define UI_PAGE_DOT_SIZE 10
#define UI_PAGE_DOT_SPACING 8

// --------------------------------------------------------
// Temperature triangle markers (geometry)
// --------------------------------------------------------
// Temperature triangle markers (geometry + behavior)
// int UI_TEMP_TARGET_TOLERANCE_C = 0; // +/- range around target
static int ui_temp_target_tolerance_c = 3;

static constexpr int UI_TEMP_TRI_W = 16;    // adjust later
static constexpr int UI_TEMP_TRI_H = 10;    // adjust later
static constexpr int UI_TEMP_TRI_GAP_Y = 4; // Abstand zur Bar
static constexpr int UI_TEMP_LABEL_GAP_X = 8;

// END OF FILE