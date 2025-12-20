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
// Build the top bar UI (elapsed progress bar + remaining time label).

static void create_top_bar\(lv_obj_t *parent);
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
// Enable/disable WAIT button clickability and apply dimmed style when disabled.

static void ui_set_pause_enabled\(bool en);
// Set WAIT/PAUSE button label text.

static void ui_set_pause_label\(const char *txt);

// WAIT helper
// Force WAIT state in UI and stop countdown timer (used for door-open safety and pause flow).

static void countdown_stop_and_set_wait_ui\(const char *why);

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

// Set pause button background color (hex 0xRRGGBB; converted via ui_color_from_hex()).

static void ui_set_pause_bg_hex\(uint32_t rgb_hex)
{
    if (!ui.btn_pause)
        return;
    lv_obj_set_style_bg_color(ui.btn_pause, ui_color_from_hex(rgb_hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
}

// Apply WAIT/PAUSE button label + enabled-state + color based on RunState and door state.

static void pause_button_apply_ui\(RunState st, bool door_open)
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

// Convert seconds/minutes (0..59) into a clock angle (degrees).

static int calc_second_angle\(int minute)
{
    return (90 - minute * 6);
}

// Convert minutes (0..59) into a clock angle (degrees).

static int calc_minute_angle\(int minute)
{
    return (90 - minute * 6);
}

// Convert (hour, minute) into a clock angle (degrees) using fractional hour.

static int calc_hour_angle\(int hour, int minute)
{
    float hm = (float)hour + (float)(minute / 60.0f);
    return (90 - (int)(hm * 30));
}

// Format remaining time as HH:MM:SS and update the top bar label.

static void set_remaining_label_seconds\(int remaining_seconds)
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

// Recompute and apply needle endpoints in screen coordinates (lv_line with mutable points).

static void update_needle\(lv_obj_t *dial, lv_obj_t *needle, lv_point_precise_t *buf, int angle_deg, int rFrom, int rTo)
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

// Enforce door safety: door open => WAIT button disabled; door closed => enable depending on run state.

static void pause_button_update_enabled_by_door\(bool door_open)
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

// Return door state, optionally overridden by UI debug toggle.

static bool get_effective_door_open\(const OvenRuntimeState &state)
{
    return g_sim_door_override ? g_sim_door_open : state.door_open;
}

// Set HH/MM/SS needles to an explicit time value.

static void set_needles_hms\(int hh, int mm, int ss)
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

// UI-owned countdown timer tick: decrement remaining seconds and update progress + needles.

static void countdown_tick_cb\(lv_timer_t *t)
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

// One-shot timer: wait until dial has a valid size, then compute needle radii/lengths and set defaults.

static void needles_init_cb\(lv_timer_t *t)
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


// Icon tap handler: manually toggle 230V fan (e.g., manual cooldown when oven not started).

static void fan230_toggle_event_cb\(lv_event_t *e)
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


// Icon tap handler: manually toggle lamp at any time.

static void lamp_toggle_event_cb\(lv_event_t *e)
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

// Debug handler: toggle simulated door state to test WAIT/lockout behavior.

static void door_debug_toggle_event_cb\(lv_event_t *e)
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

// Compute a status color based on current temp vs target +/- tolerance.

static uint32_t temp_status_color_hex\(float cur, float tgt)
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

// LVGL animation exec callback: pulse heater icon recolor opacity.

static void heater_pulse_exec_cb\(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    // Pulse only recolor opacity so the icon stays crisp and "alive"
    lv_obj_set_style_img_recolor_opa(obj, (lv_opa_t)v, LV_PART_MAIN);
}

// Start heater icon pulse animation (no-op if already active).

static void heater_pulse_start\(lv_obj_t *heater_icon)
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

// Stop heater icon pulse animation and restore steady opacity.

static void heater_pulse_stop\(lv_obj_t *heater_icon)
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

// Configure a label as a single clipped line to avoid multi-line layout shifts.

static void ui_label_set_singleline_clip\(lv_obj_t *lbl)
{
    if (!lbl)
        return;
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP); // one line, hard clip
    lv_obj_set_width(lbl, LV_PCT(100));              // use parent width
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

// Apply font selection for preset name based on measured text width, then set label text.

static void preset_name_apply_fit\(lv_obj_t *label, const char *text)
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

// Public API: runtime update
// Push latest OvenRuntimeState into the UI (called periodically by the UI manager).

void screen_main_update_runtime\(const OvenRuntimeState *state)
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
// Update the page indicator dots to reflect the currently active UI page.

void screen_main_set_active_page\(uint8_t page_index)
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

// ... (rest of file unchanged by this milestone) ...
