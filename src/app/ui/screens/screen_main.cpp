#include "screen_main.h"
#include "../icons/icons_32x32.h"
// -------------------------------------------------------------------
//
// Main-Screen
//
// Der mainScreen ist in vier Container aufgeteilt (von oben nach unten)
// Container 1:
// Anzeige ProgressBar der abgelaufen Zeit. Je länger der Balken desto
// kleiner remaining-Time
//
// Container 2:
//
//
// -------------------------------------------------------------------

constexpr int COUNT_TICK_UPDATE_FREQ = 1000;

// --------------------------------------------------------
// States
// --------------------------------------------------------
enum class RunState : uint8_t
{
    STOPPED = 0,
    RUNNING,
    WAIT
};

// Internal widget storage
// diese Struktur enthält alle Widgets von allen Screens
typedef struct main_screen_widgets_t
{
    lv_obj_t *root;

    // --------------------------------------------------------
    // Containers
    // --------------------------------------------------------
    lv_obj_t *top_bar_container;
    lv_obj_t *center_container;
    lv_obj_t *page_indicator_container;
    lv_obj_t *bottom_container;

    // --------------------------------------------------------
    // Top bar
    // --------------------------------------------------------
    lv_obj_t *time_bar;
    lv_obj_t *time_label_remaining;

    // --------------------------------------------------------
    // Center
    // --------------------------------------------------------
    lv_obj_t *icons_container;
    lv_obj_t *dial_container;
    lv_obj_t *start_button_container;

    // --------------------------------------------------------
    // Icons
    // --------------------------------------------------------
    lv_obj_t *icon_fan12v;
    lv_obj_t *icon_fan230;
    lv_obj_t *icon_fan230_slow;
    lv_obj_t *icon_heater;
    lv_obj_t *icon_door;
    lv_obj_t *icon_motor;
    lv_obj_t *icon_lamp;

    // --------------------------------------------------------
    // Dial
    // --------------------------------------------------------
    lv_obj_t *dial;
    lv_obj_t *needleSS;
    lv_obj_t *needleMM;
    lv_obj_t *needleHH;

    lv_obj_t *preset_box;        // NEW: green rounded box in dial center
    lv_obj_t *label_preset_name; // NEW: preset name (top line)

    lv_obj_t *label_preset_id; // keep: will become "#<id>" (second line, dimmed)
    lv_obj_t *label_time_in_dial;

    int32_t hours, minutes;

    // --------------------------------------------------------
    // Start/Stop
    // --------------------------------------------------------
    lv_obj_t *btn_start;
    lv_obj_t *label_btn_start;

    // --------------------------------------------------------
    // Pause/Wait (Step 1)
    // --------------------------------------------------------
    lv_obj_t *btn_pause;
    lv_obj_t *label_btn_pause;

    // --------------------------------------------------------
    // Page indicator
    // --------------------------------------------------------
    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];

    // --------------------------------------------------------
    // Bottom: temperature
    // --------------------------------------------------------
    // Bottom: temperature
    lv_obj_t *temp_scale;

    // Legacy markers (will be removed after triangle rollout)
    lv_obj_t *temp_tri_target;  // old thin line marker
    lv_obj_t *temp_tri_current; // old rectangle marker

    // Labels near triangles
    lv_obj_t *temp_label_target;  // label text near triangle
    lv_obj_t *temp_label_current; // label text near triangle

    // --------------------------------------------------------
    // Needle animation timer
    // --------------------------------------------------------
    lv_timer_t *needles_init_timer;
    int needle_rFromMinute, needle_rToMinute;
    int needle_rFromHour, needle_rToHour;

} main_screen_widgets_t;

// Static instance
static main_screen_widgets_t ui;

// Forward declarations
static void create_top_bar(lv_obj_t *parent);
static void create_center_section(lv_obj_t *parent);
static void create_page_indicator(lv_obj_t *parent);
static void create_bottom_section(lv_obj_t *parent);

static void update_time_ui(const OvenRuntimeState &state);
static void update_dial_ui(const OvenRuntimeState &state);
static void update_temp_ui(const OvenRuntimeState &state);
static void update_actuator_icons(const OvenRuntimeState &state);
static void update_start_button_ui(void);

static void start_button_event_cb(lv_event_t *e);
static void pause_button_event_cb(lv_event_t *e);

// UI helpers (needed before first use)
static void ui_set_pause_enabled(bool en);
static void ui_set_pause_label(const char *txt);

// WAIT helper
static void countdown_stop_and_set_wait_ui(const char *why);

static lv_style_t style_dial_border;

static lv_point_precise_t g_minute_hand_points[2];
static lv_point_precise_t g_hour_hand_points[2];
static lv_point_precise_t g_second_hand_points[2];

static int g_remaining_seconds = 0;
static int g_total_seconds = g_remaining_seconds;

static lv_timer_t *g_countdown_tick = nullptr;

static RunState g_run_state = RunState::STOPPED;

// Last runtime snapshot from oven_get_runtime_state()
static OvenRuntimeState g_last_runtime = {};

// Snapshot of actuator states before entering WAIT
static OvenRuntimeState g_pre_wait_snapshot = {};
static bool g_has_pre_wait_snapshot = false;

static bool g_sim_door_override = false;
static bool g_sim_door_open = false;
static bool g_paused_by_door = false;

static void ui_set_pause_bg_hex(uint32_t rgb_hex)
{
    if (!ui.btn_pause)
        return;
    lv_obj_set_style_bg_color(ui.btn_pause, ui_color_from_hex(rgb_hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
}

static void pause_button_apply_ui(RunState st, bool door_open)
{
    if (!ui.btn_pause || !ui.label_btn_pause)
        return;

    switch (st)
    {
    case RunState::STOPPED:
        ui_set_pause_label("WAIT");
        ui_set_pause_enabled(false);
        ui_set_pause_bg_hex(UI_COL_PAUSE_DISABLED_HEX);
        break;

    case RunState::RUNNING:
        ui_set_pause_label("PAUSE");
        ui_set_pause_enabled(true);
        ui_set_pause_bg_hex(UI_COL_PAUSE_RUNNING_HEX);
        break;

    case RunState::WAIT:
        ui_set_pause_label("WAIT");

        if (door_open)
        {
            // blocked by door
            ui_set_pause_enabled(false);
            ui_set_pause_bg_hex(UI_COL_PAUSE_WAIT_BLOCKED_HEX);
        }
        else
        {
            // ready to resume
            ui_set_pause_enabled(true);
            ui_set_pause_bg_hex(UI_COL_PAUSE_WAIT_READY_HEX);
        }
        break;
    }
}

static int calc_second_angle(int minute)
{
    return (90 - minute * 6);
}

static int calc_minute_angle(int minute)
{
    return (90 - minute * 6);
}

static int calc_hour_angle(int hour, int minute)
{
    float hm = (float)hour + (float)(minute / 60.0f);
    return (90 - (int)(hm * 30));
}

static void set_remaining_label_seconds(int remaining_seconds)
{
    if (remaining_seconds < 0)
        remaining_seconds = 0;

    int hh = remaining_seconds / 3600;
    int mm = (remaining_seconds % 3600) / 60;
    int ss = remaining_seconds % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);

    lv_label_set_text(ui.time_label_remaining, buf);
}

static void update_needle(lv_obj_t *dial, lv_obj_t *needle, lv_point_precise_t *buf, int angle_deg, int rFrom, int rTo)
{
    lv_area_t a;
    lv_obj_get_coords(dial, &a);

    lv_coord_t cx = (a.x1 + a.x2) / 2;
    lv_coord_t cy = (a.y1 + a.y2) / 2;

    float rad = angle_deg * 0.01745329252f; // PI/180

    buf[0].x = cx + (int)(cosf(rad) * rFrom);
    buf[0].y = cy - (int)(sinf(rad) * rFrom);

    buf[1].x = cx + (int)(cosf(rad) * rTo);
    buf[1].y = cy - (int)(sinf(rad) * rTo);

    // With mutable points this is usually enough, but this also refreshes bounds reliably.
    lv_line_set_points(needle, buf, 2);
}

static void pause_button_update_enabled_by_door(bool door_open)
{
    if (!ui.btn_pause)
        return;

    // Door-open => Pause button must not be clickable (cannot resume)
    // Door-closed => Pause button may be clickable (depending on run state)
    if (door_open)
    {
        ui_set_pause_enabled(false);
        UI_INFO("[DOOR] btnPause DISABLED (door open)\n");
    }
    else
    {
        // Only enable if we are in RUNNING or WAIT; STOPPED remains disabled elsewhere
        bool allow = (g_run_state == RunState::RUNNING) || (g_run_state == RunState::WAIT);
        ui_set_pause_enabled(allow);
        UI_INFO("[DOOR] btnPause %s (door closed)\n", allow ? "ENABLED" : "DISABLED");
    }
}

static bool get_effective_door_open(const OvenRuntimeState &state)
{
    return g_sim_door_override ? g_sim_door_open : state.door_open;
}

static void set_needles_hms(int hh, int mm, int ss)
{
    update_needle(ui.dial, ui.needleMM, g_minute_hand_points,
                  calc_minute_angle(mm),
                  ui.needle_rFromMinute, ui.needle_rToMinute);

    update_needle(ui.dial, ui.needleHH, g_hour_hand_points,
                  calc_hour_angle(hh, mm),
                  ui.needle_rFromHour, ui.needle_rToHour);

    update_needle(ui.dial, ui.needleSS, g_second_hand_points,
                  calc_minute_angle(ss),
                  ui.needle_rFromMinute, ui.needle_rToMinute);
}

static void countdown_tick_cb(lv_timer_t *t)
{
    if (g_total_seconds > 0)
    {
        int elapsed = g_total_seconds - g_remaining_seconds;
        if (elapsed < 0)
            elapsed = 0;
        if (elapsed > g_total_seconds)
            elapsed = g_total_seconds;

        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    }

    if (g_remaining_seconds <= 0)
    {
        // Countdown finished
        g_remaining_seconds = 0;

        update_needle(ui.dial, ui.needleMM, g_minute_hand_points,
                      calc_minute_angle(0),
                      ui.needle_rFromMinute, ui.needle_rToMinute);

        update_needle(ui.dial, ui.needleHH, g_hour_hand_points,
                      calc_hour_angle(0, 0),
                      ui.needle_rFromHour, ui.needle_rToHour);

        update_needle(ui.dial, ui.needleSS, g_second_hand_points,
                      calc_minute_angle(0),
                      ui.needle_rFromMinute, ui.needle_rToMinute);

        lv_timer_del(g_countdown_tick);
        g_countdown_tick = nullptr;

        UI_INFO("*************************************\n");
        UI_INFO("[COUNTDOWN] finished\n");
        UI_INFO("*************************************\n");
        return;
    }

    g_remaining_seconds--;

    if (g_total_seconds > 0)
    {
        int elapsed = g_total_seconds - g_remaining_seconds;
        if (elapsed < 0)
            elapsed = 0;
        if (elapsed > g_total_seconds)
            elapsed = g_total_seconds;

        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    }

    set_remaining_label_seconds(g_remaining_seconds);

    // --- derive HH / MM / SS ---
    int hh = g_remaining_seconds / 3600;
    int mm = (g_remaining_seconds % 3600) / 60;
    int ss = g_remaining_seconds % 60;

    // --- compute angles ---
    int angMM = calc_minute_angle(mm);
    int angHH = calc_hour_angle(hh, mm);
    int angSS = calc_minute_angle(ss);

    // --- update needles ---
    update_needle(ui.dial, ui.needleMM, g_minute_hand_points,
                  angMM, ui.needle_rFromMinute, ui.needle_rToMinute);

    update_needle(ui.dial, ui.needleHH, g_hour_hand_points,
                  angHH, ui.needle_rFromHour, ui.needle_rToHour);

    update_needle(ui.dial, ui.needleSS, g_second_hand_points,
                  angSS, ui.needle_rFromMinute, ui.needle_rToMinute);
}

static void needles_init_cb(lv_timer_t *t)
{

    // Force layout to be up-to-date
    lv_obj_update_layout(ui.root);
    lv_obj_update_layout(ui.dial);

    lv_coord_t w = lv_obj_get_width(ui.dial);
    lv_coord_t h = lv_obj_get_height(ui.dial);

    UI_INFO("[needles_init] dial size: %d x %d\n", (int)w, (int)h);

    if (w <= 0 || h <= 0)
    {
        UI_INFO("[needles_init] dial not ready yet\n");
        return; // keep timer, try again next tick
    }

    lv_coord_t dial_r = w / 2;

    static constexpr int NEEDLE_END_GAP_PX = 40;            // abstand des Zeigerendes bis zum Rand
    static constexpr float MINUTE_LEN_F = 0.750f;           // 0.75 vom Radius
    static constexpr float HOUR_LEN_FACTOR = (2.0f / 3.0f); // Stundenzeiger hat die Länge 2/3 des minuten Zeigers

    int rToMinute = (int)(dial_r - NEEDLE_END_GAP_PX);
    int lenMinute = (int)(dial_r * MINUTE_LEN_F);
    int rFromMinute = rToMinute - lenMinute;

    int lenHour = (int)(lenMinute * HOUR_LEN_FACTOR);
    int rFromHour = rFromMinute;
    int rToHour = rFromHour + lenHour;

    if (rToMinute < 1)
        rToMinute = 1;
    if (rFromMinute < 0)
        rFromMinute = 0;
    if (rToHour < 1)
        rToHour = 1;
    if (rFromHour < 0)
        rFromHour = 0;

    ui.needle_rFromMinute = rFromMinute;
    ui.needle_rToMinute = rToMinute;
    ui.needle_rFromHour = rFromHour;
    ui.needle_rToHour = rToHour;

    UI_INFO("[needles_init] rM %d..%d  rH %d..%d\n", rFromMinute, rToMinute, rFromHour, rToHour);
    // -------------------------------------------------
    // Default time after screen creation: 01:50:00
    // -------------------------------------------------
    int def_hh = 1;
    int def_mm = 50;
    int def_ss = 0;

    // Initialize at 12 o'clock
    update_needle(ui.dial, ui.needleMM, g_minute_hand_points, calc_minute_angle(def_mm), rFromMinute, rToMinute);
    update_needle(ui.dial, ui.needleHH, g_hour_hand_points, calc_hour_angle(def_hh, def_mm), rFromHour, rToHour);
    update_needle(ui.dial, ui.needleSS, g_second_hand_points, calc_minute_angle(def_ss), rFromMinute, rToMinute);
    g_remaining_seconds = def_hh * 3600 + def_mm * 60 + def_ss;
    UI_INFO("[INIT] Countdown start seconds = %d", g_remaining_seconds);

    UI_INFO("[needles_init] MM p0=%d,%d p1=%d,%d\n",
            (int)g_minute_hand_points[0].x, (int)g_minute_hand_points[0].y,
            (int)g_minute_hand_points[1].x, (int)g_minute_hand_points[1].y);

    // Progressbar update
    g_total_seconds = g_remaining_seconds;

    // Progressbar init: 0 .. total, value=0
    lv_bar_set_range(ui.time_bar, 0, g_total_seconds);
    lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);

    // One-shot: stop timer now
    lv_timer_del(ui.needles_init_timer);
    ui.needles_init_timer = nullptr;
}

static lv_obj_t *mk_scale_needle_mutable(lv_obj_t *parent,
                                         lv_point_precise_t *pts,
                                         uint8_t width,
                                         lv_color_t color)
{
    lv_obj_t *line = lv_line_create(parent);

    // Remove theme styles
    lv_obj_remove_style_all(line);

    // IMPORTANT: mutable points so we can update the buffer directly
    lv_line_set_points_mutable(line, pts, 2);

    // Coordinate space = full screen
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_size(line, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);

    lv_obj_add_flag(line, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // ---- RE-APPLY LINE STYLE (THIS IS REQUIRED) ----
    lv_obj_set_style_line_width(line, width, LV_PART_MAIN);
    lv_obj_set_style_line_color(line, color, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    lv_obj_set_style_line_opa(line, LV_OPA_COVER, LV_PART_MAIN);

    // Object itself must be invisible
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);

    return line;
}

//----------------------------------------------------
//
//----------------------------------------------------

static void countdown_stop_and_set_wait_ui(const char *why)
{
    UI_INFO("[WAIT] stop countdown: %s (tick=%p)\n", why, g_countdown_tick);

    if (g_countdown_tick)
    {
        lv_timer_del(g_countdown_tick);
        g_countdown_tick = nullptr;
        UI_INFO("[WAIT] countdown timer deleted\n");
    }

    g_run_state = RunState::WAIT;

    ui_set_pause_label("WAIT");
    ui_set_pause_enabled(false); // stays disabled until door is closed

    UI_INFO("[WAIT] state=WAIT, btnPause WAIT + DISABLED\n");
}

static void ui_set_pause_enabled(bool en)
{
    if (!ui.btn_pause)
        return;

    if (en)
    {
        lv_obj_add_flag(ui.btn_pause, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
    }
    else
    {
        lv_obj_clear_flag(ui.btn_pause, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(ui.btn_pause, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_40, LV_PART_MAIN);
    }
}

static void ui_set_pause_label(const char *txt)
{
    if (!ui.label_btn_pause)
        return;
    lv_label_set_text(ui.label_btn_pause, txt);
}


static void fan230_toggle_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;

    // Toggle fan230 state
    UI_INFO("[FAN230] 1 toggled (run_state=%d)\n", (int)g_run_state);
    oven_fan230_toggle_manual();

    UI_INFO("[FAN230] 2 toggled (run_state=%d)\n", (int)g_run_state);

    // Refresh icons immediately
    update_actuator_icons(g_last_runtime);
}


static void lamp_toggle_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;

    // Toggle lamp state
    oven_lamp_toggle_manual();

    UI_INFO("[LAMP] toggled (run_state=%d)\n", (int)g_run_state);

    // Refresh icons immediately
    update_actuator_icons(g_last_runtime);
}

static void door_debug_toggle_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;

    // Enable override and toggle simulated door
    g_sim_door_override = true;
    g_sim_door_open = !g_sim_door_open;

    // Effective door state is the simulated one now
    const bool door_open = g_sim_door_open;

    UI_INFO("[DEBUG] SIM door_open=%d (run_state=%d tick=%p)\n",
            (int)door_open, (int)g_run_state, g_countdown_tick);

    // If door opens while running -> force WAIT immediately + stop oven (safety mock)
    if (door_open && g_run_state == RunState::RUNNING)
    {
        // snapshot for later resume (actuators etc.)
        g_pre_wait_snapshot = g_last_runtime;
        g_has_pre_wait_snapshot = true;

        countdown_stop_and_set_wait_ui("door opened");
        oven_pause_wait();
        g_run_state = RunState::WAIT;
        update_start_button_ui();

        g_paused_by_door = true;
    }

    // Update pause availability based on door state
    pause_button_update_enabled_by_door(door_open);

    // Refresh icons immediately (door icon uses get_effective_door_open())
    update_actuator_icons(g_last_runtime);
}

static uint32_t temp_status_color_hex(float cur, float tgt)
{
    const float lo = tgt - (float)ui_temp_target_tolerance_c;
    const float hi = tgt + (float)ui_temp_target_tolerance_c;

    if (cur < lo)
        return UI_COLOR_TEMP_COLD_HEX; // blue
    if (cur > hi)
        return UI_COLOR_TEMP_HOT_HEX; // orange
    return UI_COLOR_TEMP_OK_HEX;      // green
}

static bool g_heater_pulse_active = false;

static void heater_pulse_exec_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    // Pulse only recolor opacity so the icon stays crisp and "alive"
    lv_obj_set_style_img_recolor_opa(obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void heater_pulse_start(lv_obj_t *heater_icon)
{
    if (!heater_icon || g_heater_pulse_active)
        return;

    g_heater_pulse_active = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, heater_icon);
    lv_anim_set_exec_cb(&a, heater_pulse_exec_cb);
    lv_anim_set_values(&a, 140, LV_OPA_COVER); // subtle range
    lv_anim_set_time(&a, 800);                 // speed
    lv_anim_set_playback_time(&a, 800);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void heater_pulse_stop(lv_obj_t *heater_icon)
{
    if (!heater_icon || !g_heater_pulse_active)
        return;

    // Stop the animation targeting this var + exec_cb
    lv_anim_del(heater_icon, heater_pulse_exec_cb);

    // Restore to normal "ON"
    lv_obj_set_style_img_recolor_opa(heater_icon, LV_OPA_COVER, LV_PART_MAIN);

    g_heater_pulse_active = false;
}

// -----------------------------
// Preset label helpers (Step 2.5.2)
// -----------------------------

static void ui_label_set_singleline_clip(lv_obj_t *lbl)
{
    if (!lbl)
        return;
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP); // one line, hard clip
    lv_obj_set_width(lbl, LV_PCT(100));              // use parent width
}

static void ui_set_preset_name_text(const char *name)
{
    if (!ui.label_preset_name)
        return;

    const char *src = (name && name[0]) ? name : "—";

    // Conservative: word-based truncation with "..."
    // NOTE: Adjust these constexpr values as you like.
    static constexpr int UI_PRESET_NAME_MAX_CHARS = 18;

    static char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", src);

    const int len = (int)std::strlen(buf);
    if (len > UI_PRESET_NAME_MAX_CHARS)
    {
        // Cut at max chars
        buf[UI_PRESET_NAME_MAX_CHARS] = '\0';

        // Try to cut at last whitespace for nicer truncation
        char *last_space = std::strrchr(buf, ' ');
        if (last_space && last_space > buf + 4) // keep at least some chars
        {
            *last_space = '\0';
        }

        // Append ellipsis (ensure room)
        const size_t cur = std::strlen(buf);
        if (cur + 3 < sizeof(buf))
        {
            std::strcat(buf, "...");
        }
    }

    lv_label_set_text(ui.label_preset_name, buf);
}

static void ui_set_preset_id(uint32_t id)
{
    if (!ui.label_preset_id)
        return;

    char filament_buf[16];
    std::snprintf(filament_buf, sizeof(filament_buf), "#%u", (unsigned)id);
    lv_label_set_text(ui.label_preset_id, filament_buf);
}

static const lv_font_t *pick_preset_font_for_width(const char *text, lv_coord_t max_w)
{
    if (!text || !*text)
        return ui_preset_font_l();

    const lv_font_t *fonts[] = {ui_preset_font_l(), ui_preset_font_m(), ui_preset_font_s()};

    for (auto f : fonts)
    {
        lv_point_t sz;
        lv_txt_get_size(&sz, text, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (sz.x <= max_w)
            return f;
    }
    return ui_preset_font_s();
}

static void preset_name_apply_fit(lv_obj_t *label, const char *text)
{
    if (!label || !text || !text[0])
        return;

    lv_obj_t *box = lv_obj_get_parent(label);
    lv_obj_update_layout(box);

    lv_coord_t box_w = lv_obj_get_width(box);
    lv_coord_t pad_l = lv_obj_get_style_pad_left(box, LV_PART_MAIN);
    lv_coord_t pad_r = lv_obj_get_style_pad_right(box, LV_PART_MAIN);
    lv_coord_t border = lv_obj_get_style_border_width(box, LV_PART_MAIN);

    lv_coord_t max_w = box_w - pad_l - pad_r - 2 * border;
    if (max_w < 10)
        max_w = 10;

    const lv_font_t *font = ui_preset_font_s(); // fallback

    lv_point_t sz;

    lv_txt_get_size(&sz, text, ui_preset_font_l(), 0, 0,
                    LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (sz.x <= max_w)
        font = ui_preset_font_l();
    else
    {
        lv_txt_get_size(&sz, text, ui_preset_font_m(), 0, 0,
                        LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (sz.x <= max_w)
            font = ui_preset_font_m();
    }

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_text(label, text);

    // if (!label)
    //     return;

    // // Ensure label uses full inner width so center-alignment works correctly
    // lv_obj_set_width(label, UI_PRESET_BOX_NAME_MAX_W);

    // const lv_font_t *f = pick_preset_font_for_width(text, UI_PRESET_BOX_NAME_MAX_W);
    // lv_obj_set_style_text_font(label, f, LV_PART_MAIN);

    // // Center text inside the label
    // lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}
//----------------------------------------------------
//
//----------------------------------------------------
// Public API: create the main screen
lv_obj_t *screen_main_create(void)
{

    // Root object
    if (ui.root != nullptr)
    {
        UI_INFO("return screen_main_create()\n");
        return ui.root;
    }

    ui.root = lv_obj_create(nullptr);
    // ui.root = lv_screen_active();
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);

    lv_obj_set_style_bg_color(ui.root, ui_color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);
    UI_DBG("[screen_main_create screen_main_create] screen-addr: %d\n", ui.root);

    lv_style_init(&style_dial_border);
    lv_style_set_border_width(&style_dial_border, 1);
    lv_style_set_border_color(&style_dial_border, lv_color_hex(0xFFFFFF)); // Weißer Rahmen
    lv_style_set_border_opa(&style_dial_border, LV_OPA_COVER);
    lv_style_set_radius(&style_dial_border, 12); // Optional: leicht abgerundet
    lv_style_set_pad_all(&style_dial_border, 0);

    // Create sub sections
    create_top_bar(ui.root);
    create_center_section(ui.root);
    create_page_indicator(ui.root);
    create_bottom_section(ui.root);

    UI_DBG("[screen_main_create screen_main_create] screen-addr: %d\n", ui.root);
    return ui.root;
}

//----------------------------------------------------
//
//----------------------------------------------------
// Public API: runtime update
//
void screen_main_update_runtime(const OvenRuntimeState *state)
{
    if (!state)
        return;

    static bool last_door_open = false;

    if (state->door_open != last_door_open)
    {
        UI_INFO("[DOOR] state changed: %d -> %d (running=%d)\n",
                (int)last_door_open, (int)state->door_open, (int)state->running);

        last_door_open = state->door_open;

        // Always keep pause button consistent with door
        pause_button_update_enabled_by_door(state->door_open);

        // If door opens while countdown is running -> force WAIT immediately
        if (state->door_open)
        {
            countdown_stop_and_set_wait_ui("door opened");
        }
    }
    g_last_runtime = *state;
    ui_temp_target_tolerance_c = state->tempToleranceC;

    update_time_ui(*state);
    update_dial_ui(*state);
    update_temp_ui(*state);
    update_actuator_icons(*state);
    update_start_button_ui();

    pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
}

// Public API: page indicator update
void screen_main_set_active_page(uint8_t page_index)
{
    if (page_index >= UI_PAGE_COUNT)
    {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        lv_color_t col = (i == page_index)
                             ? ui_color_from_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : ui_color_from_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }
}

//----------------------------------------------------
// Section creation (Container)
//----------------------------------------------------

//----------------------------------------------------
//
//----------------------------------------------------

static void create_top_bar(lv_obj_t *parent)
{
    ui.top_bar_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.top_bar_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.top_bar_container, UI_SCREEN_WIDTH, 50);
    lv_obj_align(ui.top_bar_container, LV_ALIGN_TOP_MID, 0, UI_TOP_PADDING);

    lv_obj_set_style_bg_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.top_bar_container, 0, 0);

    // Time progress bar
    ui.time_bar = lv_bar_create(ui.top_bar_container);
    lv_obj_set_size(ui.time_bar, UI_TIME_BAR_WIDTH, UI_TIME_BAR_HEIGHT);
    lv_obj_align(ui.time_bar, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING, 0);
    lv_bar_set_range(ui.time_bar, 0, 0); //
    lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.time_bar, ui_color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.time_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.time_bar, ui_color_from_hex(UI_COLOR_TIME_BAR_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.time_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    // Remaining time label on the right
    ui.time_label_remaining = lv_label_create(ui.top_bar_container);
    lv_label_set_text(ui.time_label_remaining, "00:00:00");
    lv_obj_align(ui.time_label_remaining, LV_ALIGN_RIGHT_MID, -UI_SIDE_PADDING, 0);
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_center_section(lv_obj_t *parent)
{
    ui.center_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.center_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.center_container, UI_SCREEN_WIDTH, UI_DIAL_SIZE + 20);
    lv_obj_align(ui.center_container, LV_ALIGN_CENTER, 0, -10); // slight up offset

    lv_obj_set_style_bg_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.center_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.center_container, 0, 0);

    //
    // Icons container (left)
    //
    ui.icons_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.icons_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.icons_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.icons_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.icons_container, LV_OPA_TRANSP, 0);

    lv_obj_set_flex_flow(ui.icons_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.icons_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // 32x32 white icons; we recolor them at runtime
    ui.icon_fan12v = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan12v, &fan12v_wht);
    lv_obj_set_size(ui.icon_fan12v, 32, 32);
    // fan12v is display-only: no click handler

    ui.icon_fan230 = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan230, &fan230v_fast_wht);
    lv_obj_set_size(ui.icon_fan230, 32, 32);
    lv_obj_add_flag(ui.icon_fan230, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.icon_fan230, fan230_toggle_event_cb, LV_EVENT_CLICKED, nullptr);

    ui.icon_fan230_slow = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_fan230_slow, &fan230v_low_wht);
    lv_obj_set_size(ui.icon_fan230_slow, 32, 32);

    ui.icon_heater = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_heater, &heater_wht);
    lv_obj_set_size(ui.icon_heater, 32, 32);

    ui.icon_door = lv_image_create(ui.icons_container);
    // For door we currently reuse the motor icon as placeholder; adjust if a dedicated door icon is added later
    lv_image_set_src(ui.icon_door, &door_open_wht);
    lv_obj_set_size(ui.icon_door, 32, 32);
    lv_obj_add_flag(ui.icon_door, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.icon_door, door_debug_toggle_event_cb, LV_EVENT_CLICKED, nullptr);

    ui.icon_motor = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_motor, &motor230v);
    lv_obj_set_size(ui.icon_motor, 32, 32);

    ui.icon_lamp = lv_image_create(ui.icons_container);
    lv_image_set_src(ui.icon_lamp, &lamp230v_wht);
    lv_obj_set_size(ui.icon_lamp, 32, 32);
    // Lamp is user-interactive (touch toggles state), so keep its event callback wiring
    lv_obj_add_flag(ui.icon_lamp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.icon_lamp, lamp_toggle_event_cb, LV_EVENT_CLICKED, nullptr);

    //
    // Dial container (center)
    //
    ui.dial_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.dial_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui.dial_container, &style_dial_border, 0);
    lv_obj_set_size(ui.dial_container, UI_DIAL_SIZE - 1, UI_DIAL_SIZE - 1);
    lv_obj_align(ui.dial_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui.dial_container, LV_OPA_TRANSP, 0);

    // UHR
    ui.dial = lv_scale_create(ui.dial_container);
    lv_obj_clear_flag(ui.dial, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.dial, UI_DIAL_SIZE - 10, UI_DIAL_SIZE - 10);
    lv_obj_set_style_radius(ui.dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(ui.dial, true, 0);
    lv_obj_align(ui.dial, LV_ALIGN_CENTER, 0, 0);

    // lv_obj_center(ui.dial);
    lv_scale_set_mode(ui.dial, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(ui.dial, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(ui.dial, ui_color_from_hex(UI_COLOR_DIAL), 0);
    lv_scale_set_label_show(ui.dial, true);
    lv_scale_set_total_tick_count(ui.dial, 61);
    lv_scale_set_major_tick_every(ui.dial, UI_DIAL_MIN_TICKS);
    static const char *hour_ticks[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", NULL};
    lv_scale_set_text_src(ui.dial, hour_ticks);
    static lv_style_t indicator_style;
    lv_style_init(&indicator_style);

    // /* Label style properties */
    lv_style_set_text_font(&indicator_style, LV_FONT_DEFAULT);
    // lv_style_set_text_color(&indicator_style, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_color(&indicator_style, ui_color_from_hex(UI_COLOR_DIAL_LABELS_HEX));

    // /* Major tick properties */
    //    lv_style_set_line_color(&indicator_style, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_line_color(&indicator_style, ui_color_from_hex(UI_COLOR_DIAL_TICKS_MAJOR_HEX));
    lv_style_set_length(&indicator_style, 16);    /* tick length */
    lv_style_set_line_width(&indicator_style, 3); /* tick width */
    lv_obj_add_style(ui.dial, &indicator_style, LV_PART_INDICATOR);

    // /* Minor tick properties */
    static lv_style_t minor_ticks_style;
    lv_style_init(&minor_ticks_style);
    // lv_style_set_line_color(&minor_ticks_style, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_line_color(&minor_ticks_style, ui_color_from_hex(UI_COLOR_DIAL_TICKS_MINOR_HEX));

    lv_style_set_length(&minor_ticks_style, 12);    /* tick length */
    lv_style_set_line_width(&minor_ticks_style, 2); /* tick width */
    lv_obj_add_style(ui.dial, &minor_ticks_style, LV_PART_ITEMS);

    // /* Main line properties */
    static lv_style_t main_line_style;
    lv_style_init(&main_line_style);
    lv_style_set_arc_color(&main_line_style, /*lv_color_black()*/ ui_color_from_hex(UI_COLOR_DIAL_FRAME));
    lv_style_set_arc_width(&main_line_style, 8);
    lv_obj_add_style(ui.dial, &main_line_style, LV_PART_MAIN);

    lv_scale_set_range(ui.dial, 0, 60);
    lv_scale_set_angle_range(ui.dial, 360);
    lv_scale_set_rotation(ui.dial, 270); // 0 = 3Uhr, 90=6Uhr, 180=9Uhr, 270=12Uhr

    UI_INFO("SETUP NEEDLS\n");

    // --------------------------------------------------------
    // Needles as lv_line with mutable points (no PNG)
    // --------------------------------------------------------

    // Dial radius (based on actual object size)
    lv_coord_t dial_r = lv_obj_get_width(ui.dial) / 2;

    // UI_INFO("needleMM - mk_scale_needle_mutable \n");
    //  Minute needle: thin white
    ui.needleMM = mk_scale_needle_mutable(ui.root, g_minute_hand_points, 5, lv_color_hex(0xFFFFFF));

    // Hour needle: thicker orange
    // UI_INFO("needleHH - mk_scale_needle_mutable \n");
    ui.needleHH = mk_scale_needle_mutable(ui.root, g_hour_hand_points, 10, lv_color_hex(0xFFFFFF));

    // Second needle: very thin red, same length as minute
    // UI_INFO("needleSS - mk_scale_needle_mutable\n");
    ui.needleSS = mk_scale_needle_mutable(ui.root, g_second_hand_points, 2, lv_color_hex(0xFF0000));

    lv_obj_move_foreground(ui.needleHH);
    lv_obj_move_foreground(ui.needleMM);
    lv_obj_move_foreground(ui.needleSS);

    ui.needles_init_timer = lv_timer_create(needles_init_cb, 50, &ui);

    // ----
    lv_obj_set_size(ui.dial, UI_DIAL_SIZE - 8, UI_DIAL_SIZE - 8);
    lv_obj_center(ui.dial);
    lv_obj_set_style_bg_opa(ui.dial, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.dial, LV_OPA_TRANSP, 0);

    // --------------------------------------------------------
    // Preset box in dial center (NEW)
    // --------------------------------------------------------

    // ui.preset_box = lv_obj_create(ui.dial_container);
    ui.preset_box = lv_obj_create(ui.root);
    lv_obj_clear_flag(ui.preset_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.preset_box, UI_PRESET_BOX_W, UI_PRESET_BOX_H);
    lv_obj_add_flag(ui.preset_box, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // Center + optional offset
    // lv_obj_align(ui.preset_box, LV_ALIGN_CENTER, 0, UI_PRESET_BOX_CENTER_Y_OFFSET);
    lv_obj_align_to(ui.preset_box, ui.dial, LV_ALIGN_CENTER, 0, UI_PRESET_BOX_CENTER_Y_OFFSET);

    // Style: green background, white border
    lv_obj_set_style_radius(ui.preset_box, UI_PRESET_BOX_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.preset_box, ui_color_from_hex(UI_PRESET_BOX_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.preset_box, (lv_opa_t)UI_PRESET_BOX_BG_OPA, LV_PART_MAIN);

    lv_obj_set_style_border_width(ui.preset_box, UI_PRESET_BOX_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.preset_box, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ui.preset_box, (lv_opa_t)UI_PRESET_BOX_BORDER_OPA, LV_PART_MAIN);

    lv_obj_set_style_pad_all(ui.preset_box, 6, LV_PART_MAIN);

    // --- Top line: preset name (placeholder for now) ---
    ui.label_preset_name = lv_label_create(ui.preset_box);
    lv_label_set_text(ui.label_preset_name, "PRESET");

    // IMPORTANT: give it a defined width, then center text inside it
    lv_obj_set_width(ui.label_preset_name, UI_PRESET_BOX_NAME_MAX_W);
    lv_obj_set_style_text_align(ui.label_preset_name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui.label_preset_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(ui.label_preset_name, (lv_opa_t)UI_PRESET_NAME_TEXT_OPA, LV_PART_MAIN);

    lv_obj_align(ui.label_preset_name, LV_ALIGN_TOP_MID, 0, UI_PRESET_TEXT_PAD_TOP);

    // initial fit for placeholder
    preset_name_apply_fit(ui.label_preset_name, "PRESET");

    // --- Second line: filament id (#<id>) dimmed ---
    ui.label_preset_id = lv_label_create(ui.preset_box);
    lv_label_set_text(ui.label_preset_id, "#---");
    lv_obj_set_style_text_color(ui.label_preset_id, ui_color_from_hex(UI_PRESET_ID_HEX), LV_PART_MAIN);
    lv_obj_set_style_text_opa(ui.label_preset_id, (lv_opa_t)UI_PRESET_ID_OPA, LV_PART_MAIN);
    // lv_obj_align(ui.label_filament, LV_ALIGN_CENTER, 0, +(UI_PRESET_TEXT_GAP_Y + 8));
    // lv_obj_align(ui.label_preset_id, LV_ALIGN_BOTTOM_MID, 0, -UI_PRESET_TEXT_PAD_BOTTOM);

    // ID: dimmer
    lv_obj_set_style_text_color(ui.label_preset_id, ui_color_from_hex(UI_PRESET_ID_HEX), 0);
    lv_obj_set_style_text_opa(ui.label_preset_id, UI_PRESET_ID_OPA, 0);

    // After creating ui.label_preset_name and ui.label_preset_id:
    ui_label_set_singleline_clip(ui.label_preset_name);
    // ui_label_set_singleline_clip(ui.label_preset_id);

    // re-algin notwendig nachdem das single-line clipping gesetzt wurde
    lv_obj_align(ui.label_preset_id, LV_ALIGN_BOTTOM_MID, 0, UI_PRESET_TEXT_PAD_BOTTOM);

    // Make sure preset box is above needles
    lv_obj_move_foreground(ui.preset_box);

    // --------------------------------------------------------
    // Start/Stop button container (right)
    // --------------------------------------------------------
    ui.start_button_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.start_button_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.start_button_container, UI_SIDE_PADDING, UI_DIAL_SIZE);
    lv_obj_align(ui.start_button_container, LV_ALIGN_RIGHT_MID, -UI_FRAME_PADDING, 0);
    lv_obj_set_style_bg_opa(ui.start_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.start_button_container, LV_OPA_TRANSP, 0);

    ui.btn_start = lv_btn_create(ui.start_button_container);

    // Stack buttons vertically (START on top, WAIT below)
    lv_obj_set_flex_flow(ui.start_button_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.start_button_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui.start_button_container, 12, 0); // spacing between buttons

    // Remove all theme styles so we have full control over the button appearance
    // lv_obj_remove_style_all(ui.btn_start);

    lv_obj_set_size(ui.btn_start, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);
    lv_obj_add_event_cb(ui.btn_start, start_button_event_cb, LV_EVENT_CLICKED, nullptr);

    // Base style for START button: solid color, no gradient
    // lv_obj_remove_style_all(ui.btn_start);
    lv_obj_set_style_radius(ui.btn_start, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.btn_start, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.btn_start, ui_color_from_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);

    ui.label_btn_start = lv_label_create(ui.btn_start);
    lv_label_set_text(ui.label_btn_start, "START");
    lv_obj_center(ui.label_btn_start);

    // --------------------------------------------------------
    // Pause/Wait button (below START)
    // --------------------------------------------------------
    ui.btn_pause = lv_btn_create(ui.start_button_container);
    lv_obj_set_size(ui.btn_pause, UI_START_BUTTON_SIZE, UI_START_BUTTON_SIZE);

    // Same look as START in stopped-state for now (we'll bind to state later)
    lv_obj_set_style_radius(ui.btn_pause, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.btn_pause, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.btn_pause, ui_color_from_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);

    ui.label_btn_pause = lv_label_create(ui.btn_pause);
    lv_label_set_text(ui.label_btn_pause, "WAIT");
    lv_obj_center(ui.label_btn_pause);
    lv_obj_add_event_cb(ui.btn_pause, pause_button_event_cb, LV_EVENT_CLICKED, nullptr);
    // Initially disabled (oven not running at boot)
    ui_set_pause_enabled(false);

    UI_INFO("Needles: MM=%p HH=%p SS=%p\n", ui.needleMM, ui.needleHH, ui.needleSS);
    UI_INFO("MM p0=%d,%d p1=%d,%d\n",
            (int)g_minute_hand_points[0].x, (int)g_minute_hand_points[0].y,
            (int)g_minute_hand_points[1].x, (int)g_minute_hand_points[1].y);
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_page_indicator(lv_obj_t *parent)
{
    ui.page_indicator_container = lv_obj_create(parent);
    lv_obj_set_size(ui.page_indicator_container, UI_SCREEN_WIDTH, UI_PAGE_INDICATOR_HEIGHT);
    lv_obj_align_to(ui.page_indicator_container, ui.center_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_set_style_bg_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_container, 0, 0);

    // Inner panel (rounded rectangle)
    ui.page_indicator_panel = lv_obj_create(ui.page_indicator_container);
    lv_obj_set_size(ui.page_indicator_panel, 100, 24); // width will auto-fit for 3 dots
    lv_obj_center(ui.page_indicator_panel);

    lv_obj_set_style_radius(ui.page_indicator_panel, 12, 0);
    lv_obj_set_style_bg_color(ui.page_indicator_panel, ui_color_from_hex(UI_COLOR_PANEL_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.page_indicator_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_panel, 4, 0);

    lv_obj_set_flex_flow(ui.page_indicator_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui.page_indicator_panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i)
    {
        ui.page_dots[i] = lv_obj_create(ui.page_indicator_panel);
        lv_obj_set_size(ui.page_dots[i], UI_PAGE_DOT_SIZE, UI_PAGE_DOT_SIZE);

        lv_obj_set_style_bg_opa(ui.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ui.page_dots[i], UI_PAGE_DOT_SIZE / 2, 0);
        lv_obj_set_style_border_opa(ui.page_dots[i], LV_OPA_TRANSP, 0);

        lv_obj_set_style_margin_left(ui.page_dots[i], (i == 0) ? 0 : UI_PAGE_DOT_SPACING, 0);

        lv_color_t col = (i == UI_PAGE_MAIN)
                             ? ui_color_from_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : ui_color_from_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }
}

//----------------------------------------------------
// Bottom section creation (temperature scale)
//----------------------------------------------------
static void create_bottom_section(lv_obj_t *parent)
{
    ui.bottom_container = lv_obj_create(parent);
    lv_obj_set_size(ui.bottom_container, UI_SCREEN_WIDTH, 70);
    lv_obj_align(ui.bottom_container, LV_ALIGN_BOTTOM_MID, 0, -UI_BOTTOM_PADDING);
    lv_obj_align(ui.bottom_container, LV_ALIGN_BOTTOM_MID, 0, UI_BOTTOM_PADDING + 10);

    lv_obj_set_style_bg_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.bottom_container, 0, 0);

    // Temperature scale (horizontal bar as base)
    ui.temp_scale = lv_bar_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_scale, UI_TEMP_SCALE_WIDTH, UI_TEMP_SCALE_HEIGHT);
    lv_obj_align(ui.temp_scale, LV_ALIGN_LEFT_MID, UI_SIDE_PADDING, -6);
    lv_bar_set_range(ui.temp_scale, UI_TEMP_MIN_C, UI_TEMP_MAX_C);
    lv_bar_set_value(ui.temp_scale, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(ui.temp_scale, ui_color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.temp_scale, LV_OPA_COVER, LV_PART_MAIN);
    // Indicator is not used as "fill" here; we will use separate markers.

    // --------------------------------------------------------
    // New: triangle markers (PNG) + labels
    // --------------------------------------------------------

    // --- Target triangle (up, below bar) ---
    ui.temp_tri_target = lv_image_create(ui.bottom_container);
    lv_image_set_src(ui.temp_tri_target, &temp_tri_up_wht);
    lv_obj_set_size(ui.temp_tri_target, UI_TEMP_TRI_W, UI_TEMP_TRI_H);
    lv_obj_add_flag(ui.temp_tri_target, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // recolor enabled (color will be set in update_temp_ui)
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_target, LV_OPA_COVER, LV_PART_MAIN);

    // Target label (white, right of triangle)
    ui.temp_label_target = lv_label_create(ui.bottom_container);
    lv_obj_set_style_text_color(ui.temp_label_target, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(ui.temp_label_target, "-- °C");
    lv_obj_add_flag(ui.temp_label_target, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // --- Current triangle (down, above bar) ---
    ui.temp_tri_current = lv_image_create(ui.bottom_container);
    lv_image_set_src(ui.temp_tri_current, &temp_tri_down_wht);
    lv_obj_set_size(ui.temp_tri_current, UI_TEMP_TRI_W, UI_TEMP_TRI_H);
    lv_obj_add_flag(ui.temp_tri_current, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // recolor enabled (color will be set in update_temp_ui)
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_current, LV_OPA_COVER, LV_PART_MAIN);

    // Current label (white, right of triangle)
    ui.temp_label_current = lv_label_create(ui.bottom_container);
    lv_obj_set_style_text_color(ui.temp_label_current, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(ui.temp_label_current, "-- °C");
    lv_obj_add_flag(ui.temp_label_current, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

//
// Runtime updates
//

//----------------------------------------------------
// update_time
// aktualisiert die Heiz-Zeit HH:MM:SS
// berechnet Restzeit
//----------------------------------------------------
static void update_time_ui(const OvenRuntimeState &state)
{
    const uint32_t totalSeconds = state.durationMinutes * 60;
    const uint32_t remaining = state.secondsRemaining;

    // UI countdown owns the top bar while running (prevents flicker with oven runtime updates)
    if (g_countdown_tick != nullptr)
    {
        return;
    }

    if (totalSeconds > 0)
    {
        uint32_t elapsed = (remaining <= totalSeconds) ? (totalSeconds - remaining) : totalSeconds;
        lv_bar_set_range(ui.time_bar, 0, totalSeconds);
        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    }
    else
    {
        lv_bar_set_range(ui.time_bar, 0, 1);
        lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);
    }

    char buf[16];
    format_hhmm(remaining, buf, sizeof(buf));
    lv_label_set_text(ui.time_label_remaining, buf);
}

//----------------------------------------------------
// update_dail
//
//----------------------------------------------------

static void update_dial_ui(const OvenRuntimeState &state)
{
    // Ensure sizes/styles are resolved before measuring
    lv_obj_update_layout(ui.preset_box);

    // Compute available width inside the preset box (content area)
    lv_coord_t box_w = lv_obj_get_width(ui.preset_box);

    lv_coord_t pad_l = lv_obj_get_style_pad_left(ui.preset_box, LV_PART_MAIN);
    lv_coord_t pad_r = lv_obj_get_style_pad_right(ui.preset_box, LV_PART_MAIN);
    lv_coord_t border_w = lv_obj_get_style_border_width(ui.preset_box, LV_PART_MAIN);

    lv_coord_t max_text_w = box_w - pad_l - pad_r - 2 * border_w;

    // Safety clamp
    if (max_text_w < 10)
        max_text_w = 10;

    // Preset name (top line)
    if (ui.label_preset_name)
    {
        lv_label_set_text(ui.label_preset_name, state.presetName);
        const char *name = state.presetName;

        const lv_font_t *f = pick_preset_font_for_width(name, max_text_w);
        lv_obj_set_style_text_font(ui.label_preset_name, f, LV_PART_MAIN);
        // lv_label_set_text(ui.label_preset_name, name);

        preset_name_apply_fit(ui.label_preset_name, name);

        // aufbereitete Log-Ausgabe ob korrekt "gemessen" wurde
        lv_point_t szL, szM, szS;
        lv_txt_get_size(&szL, name, ui_preset_font_l(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_txt_get_size(&szM, name, ui_preset_font_m(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_txt_get_size(&szS, name, ui_preset_font_s(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

        // UI_DBG("[PRESET] name='%s' max_w=%d  L=%d M=%d S=%d\n",
        //         name, (int)max_text_w, (int)szL.x, (int)szM.x, (int)szS.x);
    }

    // Filament id (second line)
    char filament_buf[16];
    std::snprintf(filament_buf, sizeof(filament_buf), "#%u", (unsigned)state.filamentId);
    if (ui.label_preset_id)
        lv_label_set_text(ui.label_preset_id, filament_buf);
}

//----------------------------------------------------
// update_temp
// update der aktuellen Temperatur-Labels
// update der Scale-für IST-Temperatur
//----------------------------------------------------
static void update_temp_ui(const OvenRuntimeState &state)
{
    float cur_f = state.tempCurrent;
    float tgt_f = state.tempTarget;

    int16_t cur = (int16_t)lroundf(cur_f);
    int16_t tgt = (int16_t)lroundf(tgt_f);

    // Example (adjust to your real field names):
    // cur = (int16_t)state.temp_current;
    // tgt = (int16_t)state.temp_target;

    if (cur < UI_TEMP_MIN_C)
        cur = UI_TEMP_MIN_C;
    if (cur > UI_TEMP_MAX_C)
        cur = UI_TEMP_MAX_C;
    if (tgt < UI_TEMP_MIN_C)
        tgt = UI_TEMP_MIN_C;
    if (tgt > UI_TEMP_MAX_C)
        tgt = UI_TEMP_MAX_C;

    // Update CURRENT label
    char buf_cur[16];
    std::snprintf(buf_cur, sizeof(buf_cur), "%d °C", (int)cur);
    lv_label_set_text(ui.temp_label_current, buf_cur);

    // Update TARGET label
    char buf_tgt[16];
    std::snprintf(buf_tgt, sizeof(buf_tgt), "%d °C", (int)tgt);
    lv_label_set_text(ui.temp_label_target, buf_tgt);
    // Dim target label when current temp is within tolerance range
    {
        const int tol = ui_temp_target_tolerance_c;
        const bool in_range = (cur >= (tgt - tol)) && (cur <= (tgt + tol));

        // Slightly dim target label when "OK" (within range), otherwise full opacity
        lv_obj_set_style_text_opa(ui.temp_label_target,
                                  in_range ? LV_OPA_60 : LV_OPA_COVER,
                                  LV_PART_MAIN);
    }

    // --- Triangle colors ---
    // Target always red
    lv_obj_set_style_img_recolor(ui.temp_tri_target, ui_color_from_hex(UI_COLOR_TEMP_TARGET_HEX), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_target, LV_OPA_COVER, LV_PART_MAIN);

    // Current depends on temperature vs target range
    {
        const uint32_t hex = temp_status_color_hex(state.tempCurrent, state.tempTarget);
        lv_color_t col = ui_color_from_hex(hex);

        lv_obj_set_style_img_recolor(ui.temp_tri_current, col, LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(ui.temp_tri_current, LV_OPA_COVER, LV_PART_MAIN);
    }

    lv_coord_t scale_x = lv_obj_get_x(ui.temp_scale);
    lv_coord_t scale_y = lv_obj_get_y(ui.temp_scale);
    lv_coord_t scale_w = lv_obj_get_width(ui.temp_scale);
    lv_coord_t scale_h = lv_obj_get_height(ui.temp_scale);

    auto value_to_x = [&](int16_t value) -> lv_coord_t
    {
        float t = (float)(value - UI_TEMP_MIN_C) / (float)(UI_TEMP_MAX_C - UI_TEMP_MIN_C);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;
        return scale_x + (lv_coord_t)(t * (float)scale_w);
    };

    lv_coord_t tgt_x = value_to_x(tgt);
    lv_coord_t cur_x = value_to_x(cur);

    // Triangles centered on x
    lv_coord_t cur_tri_x = cur_x - (UI_TEMP_TRI_W / 2);
    lv_coord_t tgt_tri_x = tgt_x - (UI_TEMP_TRI_W / 2);

    // Y positions
    lv_coord_t cur_tri_y = scale_y - UI_TEMP_TRI_H - UI_TEMP_TRI_GAP_Y;
    lv_coord_t tgt_tri_y = scale_y + scale_h + UI_TEMP_TRI_GAP_Y;

    lv_obj_set_pos(ui.temp_tri_current, cur_tri_x, cur_tri_y);
    lv_obj_set_pos(ui.temp_tri_target, tgt_tri_x, tgt_tri_y);

    // Labels right of triangles
    lv_obj_set_pos(ui.temp_label_current,
                   cur_tri_x + UI_TEMP_TRI_W + UI_TEMP_LABEL_GAP_X,
                   cur_tri_y + (UI_TEMP_TRI_H / 2) - (lv_obj_get_height(ui.temp_label_current) / 2));

    lv_obj_set_pos(ui.temp_label_target,
                   tgt_tri_x + UI_TEMP_TRI_W + UI_TEMP_LABEL_GAP_X,
                   tgt_tri_y + (UI_TEMP_TRI_H / 2) - (lv_obj_get_height(ui.temp_label_target) / 2));
}

//----------------------------------------------------
// update_actuator_icons
// führt update für Icons for (Farbwechsel)
// Icons signalisieren lediglich ON/OFF
//----------------------------------------------------
static void update_actuator_icons(const OvenRuntimeState &state)
{
    auto set_icon_state = [](lv_obj_t *obj, lv_color_t on_color, bool on)
    {
        if (!obj)
        {
            return;
        }

        if (on)
        {
            // Recolor the white icon to the given ON color
            lv_obj_set_style_img_recolor(obj, on_color, LV_PART_MAIN);
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        }
        else
        {
            // OFF: show original white icon (no recolor)
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
        }
    };
    // Door icon is always live from real runtime state
    const bool door_open = get_effective_door_open(state);
    const lv_color_t col_on = ui_color_from_hex(UI_COLOR_ICON_ON_HEX);               // z.B. grün
    const lv_color_t col_door_open = ui_color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX); // z.B. rot

    // Door always reflects reality
    if (state.heater_on && g_run_state != RunState::WAIT)
    {
        const uint32_t hex = temp_status_color_hex(state.tempCurrent, state.tempTarget);
        set_icon_state(ui.icon_heater, ui_color_from_hex(hex), true);

        // Subtle pulse while heating
        heater_pulse_start(ui.icon_heater);
    }
    else
    {
        // No heating (or WAIT): no pulse
        heater_pulse_stop(ui.icon_heater);
        set_icon_state(ui.icon_heater, col_on, false);
    }

    // WAIT override: show safe-state regardless of the real actuator bits
    if (g_run_state == RunState::WAIT)
    {
        set_icon_state(ui.icon_fan230, col_on, false);     // OFF
        set_icon_state(ui.icon_fan230_slow, col_on, true); // ON
        set_icon_state(ui.icon_fan12v, col_on, true);      // ON
        set_icon_state(ui.icon_lamp, col_on, true);        // ON
        set_icon_state(ui.icon_heater, col_on, false);     // OFF
        set_icon_state(ui.icon_motor, col_on, false);      // OFF
        return;
    }
    // 12V fan: white (off) -> green (on)
    set_icon_state(ui.icon_fan12v, col_on, state.fan12v_on);

    // 230V fan fast: white (off) -> green (on)
    set_icon_state(ui.icon_fan230, col_on, state.fan230_on);

    // 230V fan slow: white (off) -> green (on)
    set_icon_state(ui.icon_fan230_slow, col_on, state.fan230_slow_on);

    // Heater: white (off) -> green (on)
    // set_icon_state(ui.icon_heater, col_on, state.heater_on);

    // Motor: white (off) -> green (on)
    set_icon_state(ui.icon_motor, col_on, state.motor_on);

    // Lamp: white (off) -> green (on)
    set_icon_state(ui.icon_lamp, col_on, state.lamp_on);
}

static void update_start_button_ui(void)
{
    if (!ui.btn_start || !ui.label_btn_start)
    {
        return;
    }

    const bool running = (g_run_state != RunState::STOPPED);

    if (running)
    {
        // Oven is running: button should be red and show STOP
        lv_label_set_text(ui.label_btn_start, "STOP");
        lv_obj_set_style_bg_color(ui.btn_start, ui_color_from_hex(UI_COLOR_DANGER_HEX), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);
    }
    else
    {
        // Oven is stopped: button should be orange and show START
        lv_label_set_text(ui.label_btn_start, "START");
        lv_obj_set_style_bg_color(ui.btn_start, ui_color_from_hex(0xFFA500), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);
    }
}

//
// Start/Stop button callback
//
//----------------------------------------------------
// start_button_event_cb
//
//----------------------------------------------------
static void start_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);

    // Wichtig: irgendein Log, das du sicher siehst
    UI_INFO("start_button_event_cb(): code=%d\n", (int)code);

    if (g_run_state != RunState::STOPPED)
    {
        // STOP from RUNNING or WAIT
        oven_stop();

        if (g_countdown_tick)
        {
            lv_timer_del(g_countdown_tick);
            g_countdown_tick = nullptr;
        }

        // Reset to default preset time (temporary until config is wired)
        int h = 1;
        int m = 50;
        g_remaining_seconds = h * 3600 + m * 60;
        g_total_seconds = g_remaining_seconds;

        set_needles_hms(h, m, 0);
        set_remaining_label_seconds(g_remaining_seconds);

        lv_bar_set_range(ui.time_bar, 0, g_total_seconds);
        lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);

        g_run_state = RunState::STOPPED;
        UI_INFO("OVEN_STOPPED\n");
        return;
    }
    // START from STOPPED
    oven_start();
    g_run_state = RunState::RUNNING;
    UI_INFO("OVEN_STARTED\n");

    // Example: ui->hours / ui->minutes come from your config

    if (g_remaining_seconds <= 0)
    {
        UI_INFO("[COUNTDOWN] nothing to start (remaining_seconds=%d)\n", g_remaining_seconds);
        return;
    }

    if (g_countdown_tick)
    {
        lv_timer_del(g_countdown_tick);
    }

    g_total_seconds = g_remaining_seconds;

    lv_bar_set_range(ui.time_bar, 0, g_total_seconds);
    lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);

    set_remaining_label_seconds(g_remaining_seconds);

    g_countdown_tick = lv_timer_create(countdown_tick_cb, COUNT_TICK_UPDATE_FREQ, &ui);
    pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
    UI_INFO("[COUNTDOWN] started: %d seconds\n", g_remaining_seconds);
}

static void pause_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (g_run_state == RunState::RUNNING)
    {
        // Enter WAIT: stop countdown timer, snapshot runtime
        if (g_countdown_tick)
        {
            lv_timer_del(g_countdown_tick);
            g_countdown_tick = nullptr;
        }

        g_pre_wait_snapshot = g_last_runtime;
        g_has_pre_wait_snapshot = true;

        g_run_state = RunState::WAIT;

        UI_INFO("[WAIT] entered\n");
        return;
    }

    if (g_run_state == RunState::WAIT)
    {
        if (get_effective_door_open(g_last_runtime))
        {
            UI_INFO("[WAIT] cannot resume: door open\n");
            return;
        }

        if (!oven_resume_from_wait())
        {
            UI_INFO("[WAIT] resume rejected by oven\n");
            return;
        }

        g_run_state = RunState::RUNNING;

        if (!g_countdown_tick)
            g_countdown_tick = lv_timer_create(countdown_tick_cb, COUNT_TICK_UPDATE_FREQ, &ui);

        UI_INFO("[WAIT] resumed\n");
        return;
    }
}

// END OF FILE