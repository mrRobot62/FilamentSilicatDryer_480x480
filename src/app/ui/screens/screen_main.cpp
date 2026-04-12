#include "screen_main.h"
#include "host_parameters.h"
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
enum class RunState : uint8_t {
    STOPPED = 0,
    RUNNING,
    WAIT
};

// Internal widget storage
// diese Struktur enthält alle Widgets von allen Screens
typedef struct main_screen_widgets_t {
    lv_obj_t *root;

    // --------------------------------------------------------
    // Containers
    // --------------------------------------------------------
    lv_obj_t *top_bar_container;
    lv_obj_t *center_container;
    lv_obj_t *page_indicator_container;
    lv_obj_t *bottom_container;

    // --------------------------------------------------------
    // Swipe screens
    // --------------------------------------------------------
    lv_obj_t *swipe_hit;
    lv_obj_t *s_swipe_target;
    // --------------------------------------------------------
    // Top bar
    // --------------------------------------------------------
    lv_obj_t *time_bar;
    lv_obj_t *time_label_remaining;

    // --------------------------------------------------------
    // Top bar 2 (marquee / notification line)
    // --------------------------------------------------------
    lv_obj_t *top_bar2_container;
    lv_obj_t *top_bar2_label;
    lv_timer_t *top_bar2_timer;
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

    // -- Synchronization icon (esp32-s3 -> esp32-wroom)
    lv_obj_t *icon_sync;

    // -- Safety icon (T13 safetyCutoffActive)
    lv_obj_t *icon_safety;

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
    // Fast preset buttons
    // --------------------------------------------------------
    lv_obj_t *btn_fast_preset[4];
    lv_obj_t *label_fast_preset[4];

    // --------------------------------------------------------
    // Page indicator
    // --------------------------------------------------------
    lv_obj_t *page_indicator_panel;
    lv_obj_t *page_dots[UI_PAGE_COUNT];

    // --------------------------------------------------------
    // Bottom: temperature
    // --------------------------------------------------------
    lv_obj_t *temp_scale_current;
    lv_obj_t *temp_scale_target;

    lv_obj_t *temp_tri_hotspot;

    // Labels near triangles
    lv_obj_t *temp_label_target;
    lv_obj_t *temp_label_current;
    lv_obj_t *temp_label_hotspot;

    lv_obj_t *temp_tol_low_line;
    lv_obj_t *temp_tol_high_line;

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
static void create_top_bar2(lv_obj_t *parent);
static void top_bar2_hide_cb(lv_timer_t *t);

// static void create_page_indicator(lv_obj_t *parent, uint8_t active_index);
static void create_page_indicator(lv_obj_t *parent);
static void create_bottom_section(lv_obj_t *parent);

static void update_status_icons(const OvenRuntimeState &state);
static void update_time_ui(const OvenRuntimeState &state);
static void update_dial_ui(const OvenRuntimeState &state);
static void update_temp_ui(const OvenRuntimeState &state);
static void update_actuator_icons(const OvenRuntimeState &state);
static void update_start_button_ui(void);
static void update_fast_preset_buttons_ui(void);

static void start_button_event_cb(lv_event_t *e);
static void pause_button_event_cb(lv_event_t *e);
static void fast_preset_button_event_cb(lv_event_t *e);

// UI helpers (needed before first use)
static void ui_set_pause_enabled(bool en);
static void ui_set_pause_label(const char *txt);

// WAIT helper
static void countdown_stop_and_set_wait_ui(const char *why);

// --------------------------------------------------------
// T10.1.39b – UI Event Edge Tracking
// --------------------------------------------------------
static bool g_prev_door_open_eff = false;
static bool g_prev_host_overtemp = false;
static bool g_runtime_edges_initialized = false;
static bool g_status_banner_initialized = false;

bool hostOvertempActive;

// =============================================================================
// TopBar2 public API
// =============================================================================
//
// Behavior:
// - Frame (rounded container) always stays visible
// - Text can be shown with optional timeout (ms)
// - If text fits: centered (no scroll)
// - If text is long: circular scrolling marquee
//
void screen_main_topbar2_show(const char *text,
                              uint32_t text_hex,
                              uint32_t bg_hex,
                              uint32_t timeout_ms);

void screen_main_topbar2_clear_text(void);

static lv_style_t style_dial_border;

static lv_point_precise_t g_minute_hand_points[2];
static lv_point_precise_t g_hour_hand_points[2];
static lv_point_precise_t g_second_hand_points[2];

static int g_remaining_seconds = 0;
static int g_total_seconds = g_remaining_seconds;

static lv_timer_t *g_countdown_tick = nullptr;

static RunState g_run_state = RunState::STOPPED;

static lv_style_t g_main_line_style; // dial frame arc style
static bool g_main_line_style_inited = false;

static OvenMode g_prev_mode_for_post_visuals = OvenMode::STOPPED;

// Last runtime snapshot from oven_get_runtime_state()
static OvenRuntimeState g_last_runtime = {};

// Snapshot of actuator states before entering WAIT
static OvenRuntimeState g_pre_wait_snapshot = {};
static bool g_has_pre_wait_snapshot = false;

static bool g_sim_door_override = false;
static bool g_sim_door_open = false;
static bool g_paused_by_door = false;

static constexpr uint16_t kFastPresetSlotCount = 4;
static uint16_t s_fast_preset_ids[kFastPresetSlotCount] = {5, 4, 3, 6}; // PLA, PETG, ASA, TPU
static constexpr uint32_t UI_COL_FAST_PRESET_BG_HEX = 0x2A2A2A;
static constexpr uint32_t UI_COL_FAST_PRESET_BG_ACTIVE_HEX = 0x3A3A3A;
static constexpr uint32_t UI_COL_FAST_PRESET_BORDER_HEX = 0x5A5A5A;
static constexpr uint32_t UI_COL_FAST_PRESET_BORDER_ACTIVE_HEX = 0xFFA500;
static constexpr uint32_t UI_COL_FAST_PRESET_TEXT_HEX = 0xFFFFFF;

static void fast_preset_label_text(uint16_t preset_id, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return;
    }

    const FilamentPreset *p = oven_get_preset(preset_id);
    if (!p || !p->name[0]) {
        std::snprintf(buf, buf_size, "----");
        return;
    }

    std::snprintf(buf, buf_size, "%.5s", p->name);
}

static void sync_fast_preset_ids_from_host_parameters(void) {
    const HostParameters *params = host_parameters_get_cached();
    if (!params) {
        return;
    }

    for (uint16_t i = 0; i < kFastPresetSlotCount; ++i) {
        s_fast_preset_ids[i] = params->shortcutPresetIds[i];
    }
}

static void ui_set_pause_bg_hex(uint32_t rgb_hex) {
    if (!ui.btn_pause) {
        return;
    }
    lv_obj_set_style_bg_color(ui.btn_pause, ui_color_from_hex(rgb_hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
}

static void pause_button_apply_ui(RunState st, bool door_open) {
    if (!ui.btn_pause || !ui.label_btn_pause) {
        return;
    }

    switch (st) {
    case RunState::STOPPED:
        ui_set_pause_label("PAUSE");
        ui_set_pause_enabled(false);
        ui_set_pause_bg_hex(UI_COL_PAUSE_DISABLED_HEX);
        break;

    case RunState::RUNNING:
        ui_set_pause_label("PAUSE");
        ui_set_pause_enabled(true);
        ui_set_pause_bg_hex(UI_COL_PAUSE_RUNNING_HEX);
        break;

    case RunState::WAIT:
        if (door_open) {
            ui_set_pause_label("CLOSE DOOR");
            ui_set_pause_enabled(false);
            ui_set_pause_bg_hex(UI_COL_PAUSE_WAIT_BLOCKED_HEX);
        } else {
            ui_set_pause_label("RESUME");
            ui_set_pause_enabled(true);
            ui_set_pause_bg_hex(UI_COL_PAUSE_WAIT_READY_HEX);
        }
        break;
    }
}

static void update_fast_preset_buttons_ui(void) {
    sync_fast_preset_ids_from_host_parameters();

    const bool enabled = (g_run_state == RunState::STOPPED);
    const uint16_t active_preset = (uint16_t)g_last_runtime.filamentId;

    for (uint16_t i = 0; i < kFastPresetSlotCount; ++i) {
        lv_obj_t *btn = ui.btn_fast_preset[i];
        lv_obj_t *label = ui.label_fast_preset[i];
        if (!btn || !label) {
            continue;
        }

        char label_buf[8];
        fast_preset_label_text(s_fast_preset_ids[i], label_buf, sizeof(label_buf));
        lv_label_set_text(label, label_buf);

        const bool is_active = (s_fast_preset_ids[i] == active_preset);
        const uint32_t bg_hex = is_active ? UI_COL_FAST_PRESET_BG_ACTIVE_HEX : UI_COL_FAST_PRESET_BG_HEX;
        const uint32_t border_hex = is_active ? UI_COL_FAST_PRESET_BORDER_ACTIVE_HEX : UI_COL_FAST_PRESET_BORDER_HEX;

        lv_obj_set_style_bg_color(btn, ui_color_from_hex(bg_hex), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, is_active ? 2 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, ui_color_from_hex(border_hex), LV_PART_MAIN);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, ui_color_from_hex(UI_COL_FAST_PRESET_TEXT_HEX), LV_PART_MAIN);

        if (enabled) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(btn, LV_OPA_40, LV_PART_MAIN);
        }
    }
}

static int calc_second_angle(int minute) {
    return (90 - minute * 6);
}

static int calc_minute_angle(int minute) {
    return (90 - minute * 6);
}

static int calc_hour_angle(int hour, int minute) {
    float hm = (float)hour + (float)(minute / 60.0f);
    return (90 - (int)(hm * 30));
}

static void set_remaining_label_seconds(int remaining_seconds) {
    if (remaining_seconds < 0) {
        remaining_seconds = 0;
    }

    int hh = remaining_seconds / 3600;
    int mm = (remaining_seconds % 3600) / 60;
    int ss = remaining_seconds % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);

    lv_label_set_text(ui.time_label_remaining, buf);
}

static void update_needle(lv_obj_t *dial, lv_obj_t *needle, lv_point_precise_t *buf, int angle_deg, int rFrom, int rTo) {
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
static void pause_button_update_enabled_by_door(bool door_open) {
    if (!ui.btn_pause) {
        return;
    }

    // Door-open => Pause button must not be clickable (cannot resume)
    // Door-closed => Pause button may be clickable (depending on run state)
    if (door_open) {
        ui_set_pause_enabled(false);
        UI_INFO("[DOOR] btnPause DISABLED (door open)\n");
    } else {
        // Only enable if we are in RUNNING or WAIT; STOPPED remains disabled elsewhere
        bool allow = (g_run_state == RunState::RUNNING) || (g_run_state == RunState::WAIT);
        ui_set_pause_enabled(allow);
        UI_INFO("[DOOR] btnPause %s (door closed)\n", allow ? "ENABLED" : "DISABLED");
    }
}

static bool get_effective_door_open(const OvenRuntimeState &state) {
    return g_sim_door_override ? g_sim_door_open : state.door_open;
}

static void set_needles_hms(int hh, int mm, int ss) {
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

void screen_main_refresh_from_runtime(void) {
    OvenRuntimeState st;
    oven_get_runtime_state(&st);

    // Ensure dial coords are valid after screen switch / unhide
    lv_obj_update_layout(ui.root);
    lv_obj_update_layout(ui.dial);

    int hh = 0, mm = 0, ss = 0;
    const bool delay_waiting = st.delayStartRuntime.active && st.delayStartRuntime.waiting;
    const bool is_active = (st.mode != OvenMode::STOPPED) || delay_waiting;

    if (!is_active) {
        // STOPPED: runtime holds the configured total duration
        const int totalSec = (int)st.durationMinutes * 60;
        g_total_seconds = totalSec;
        g_remaining_seconds = totalSec;

        hh = g_remaining_seconds / 3600;
        mm = (g_remaining_seconds % 3600) / 60;
        ss = 0;

        // Progressbar reset (0 elapsed)
        lv_bar_set_range(ui.time_bar, 0, (g_total_seconds > 0) ? g_total_seconds : 1);
        lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);

    } else {
        // RUNNING / POST / delayed start: runtime holds remaining seconds
        const int rem = delay_waiting ? (int)st.delayStartRuntime.delayRemainingSec
                                      : (int)st.secondsRemaining;
        g_remaining_seconds = (rem < 0) ? 0 : rem;

        // If durationMinutes is valid, use it as total; else fall back to remaining
        int totalSec = delay_waiting ? (int)oven_get_delay_start_seconds()
                                     : (int)st.durationMinutes * 60;
        if (totalSec <= 0) {
            totalSec = g_remaining_seconds;
        }
        g_total_seconds = totalSec;

        hh = g_remaining_seconds / 3600;
        mm = (g_remaining_seconds % 3600) / 60;
        ss = g_remaining_seconds % 60;

        // Progressbar: elapsed = total - remaining
        int elapsed = g_total_seconds - g_remaining_seconds;
        if (elapsed < 0) {
            elapsed = 0;
        }
        if (elapsed > g_total_seconds) {
            elapsed = g_total_seconds;
        }

        lv_bar_set_range(ui.time_bar, 0, (g_total_seconds > 0) ? g_total_seconds : 1);
        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    }

    // Dial is 12h styled -> wrap hour hand nicely
    const int hh12 = hh % 12;

    set_needles_hms(hh12, mm, ss);
    set_remaining_label_seconds(g_remaining_seconds);
}

static void countdown_tick_cb(lv_timer_t *t) {
    // if (g_total_seconds > 0) {
    //     int elapsed = g_total_seconds - g_remaining_seconds;
    //     if (elapsed < 0) {
    //         elapsed = 0;
    //     }
    //     if (elapsed > g_total_seconds) {
    //         elapsed = g_total_seconds;
    //     }

    //     lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    // }

    // if (g_remaining_seconds <= 0) {
    //     // Countdown finished
    //     g_remaining_seconds = 0;

    //     update_needle(ui.dial, ui.needleMM, g_minute_hand_points,
    //                   calc_minute_angle(0),
    //                   ui.needle_rFromMinute, ui.needle_rToMinute);

    //     update_needle(ui.dial, ui.needleHH, g_hour_hand_points,
    //                   calc_hour_angle(0, 0),
    //                   ui.needle_rFromHour, ui.needle_rToHour);

    //     update_needle(ui.dial, ui.needleSS, g_second_hand_points,
    //                   calc_minute_angle(0),
    //                   ui.needle_rFromMinute, ui.needle_rToMinute);

    //     lv_timer_del(g_countdown_tick);
    //     g_countdown_tick = nullptr;

    //     UI_INFO("*************************************\n");
    //     UI_INFO("[COUNTDOWN] finished\n");
    //     UI_INFO("*************************************\n");
    //     return;
    // }

    // g_remaining_seconds--;

    // if (g_total_seconds > 0) {
    //     int elapsed = g_total_seconds - g_remaining_seconds;
    //     if (elapsed < 0) {
    //         elapsed = 0;
    //     }
    //     if (elapsed > g_total_seconds) {
    //         elapsed = g_total_seconds;
    //     }

    //     lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    // }

    // set_remaining_label_seconds(g_remaining_seconds);

    // // --- derive HH / MM / SS ---
    // int hh = g_remaining_seconds / 3600;
    // int mm = (g_remaining_seconds % 3600) / 60;
    // int ss = g_remaining_seconds % 60;

    // // --- compute angles ---
    // int angMM = calc_minute_angle(mm);
    // int angHH = calc_hour_angle(hh, mm);
    // int angSS = calc_minute_angle(ss);

    // // --- update needles ---
    // update_needle(ui.dial, ui.needleMM, g_minute_hand_points,
    //               angMM, ui.needle_rFromMinute, ui.needle_rToMinute);

    // update_needle(ui.dial, ui.needleHH, g_hour_hand_points,
    //               angHH, ui.needle_rFromHour, ui.needle_rToHour);

    // update_needle(ui.dial, ui.needleSS, g_second_hand_points,
    //               angSS, ui.needle_rFromMinute, ui.needle_rToMinute);
}

static void needles_init_cb(lv_timer_t *t) {

    // Force layout to be up-to-date
    lv_obj_update_layout(ui.root);
    lv_obj_update_layout(ui.dial);

    lv_coord_t w = lv_obj_get_width(ui.dial);
    lv_coord_t h = lv_obj_get_height(ui.dial);

    UI_INFO("[needles_init] dial size: %d x %d\n", (int)w, (int)h);

    if (w <= 0 || h <= 0) {
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

    if (rToMinute < 1) {
        rToMinute = 1;
    }
    if (rFromMinute < 0) {
        rFromMinute = 0;
    }
    if (rToHour < 1) {
        rToHour = 1;
    }
    if (rFromHour < 0) {
        rFromHour = 0;
    }

    ui.needle_rFromMinute = rFromMinute;
    ui.needle_rToMinute = rToMinute;
    ui.needle_rFromHour = rFromHour;
    ui.needle_rToHour = rToHour;

    UI_INFO("[needles_init] rM %d..%d  rH %d..%d\n", rFromMinute, rToMinute, rFromHour, rToHour);
    // -------------------------------------------------
    // Default time after screen creation: 01:50:00
    // -------------------------------------------------
    int def_hh = 12;
    int def_mm = 0;
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
                                         lv_color_t color) {
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
// --- helpers: stringify OvenMode for logs ---
static const char *oven_mode_to_str(OvenMode m) {
    switch (m) {
    case OvenMode::STOPPED:
        return "STOPPED";
    case OvenMode::RUNNING:
        return "RUNNING";
    case OvenMode::WAITING:
        return "WAITING";
    case OvenMode::POST:
        return "POST";
    default:
        return "UNKNOWN";
    }
}

static void countdown_stop_and_set_wait_ui(const char *why) {
    UI_INFO("[WAIT] stop countdown: %s (tick=%p)\n", why, g_countdown_tick);

    if (g_countdown_tick) {
        lv_timer_del(g_countdown_tick);
        g_countdown_tick = nullptr;
        UI_INFO("[WAIT] countdown timer deleted\n");
    }

    g_run_state = RunState::WAIT;

    ui_set_pause_label("CLOSE DOOR");
    ui_set_pause_enabled(false); // stays disabled until door is closed

    UI_INFO("[WAIT] state=WAIT, btnPause CLOSE DOOR + DISABLED\n");
}

static void ui_set_pause_enabled(bool en) {
    if (!ui.btn_pause) {
        return;
    }

    if (en) {
        lv_obj_add_flag(ui.btn_pause, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_clear_flag(ui.btn_pause, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(ui.btn_pause, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_40, LV_PART_MAIN);
    }
}

static void ui_set_pause_label(const char *txt) {
    if (!ui.label_btn_pause) {
        return;
    }
    lv_label_set_text(ui.label_btn_pause, txt);
}

static void fan230_toggle_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    // Toggle fan230 state
    UI_INFO("[FAN230] 1 toggled (run_state=%d)\n", (int)g_run_state);
    oven_fan230_toggle_manual();

    UI_INFO("[FAN230] 2 toggled (run_state=%d)\n", (int)g_run_state);

    // Refresh icons immediately
    update_actuator_icons(g_last_runtime);
}

static void lamp_toggle_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    // Toggle lamp state
    oven_lamp_toggle_manual();

    UI_INFO("[LAMP] toggled (run_state=%d)\n", (int)g_run_state);

    // Refresh icons immediately
    update_actuator_icons(g_last_runtime);
}

// #ff5b5b

static void door_debug_toggle_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    // Enable override and toggle simulated door
    g_sim_door_override = true;
    g_sim_door_open = !g_sim_door_open;

    // Effective door state is the simulated one now
    const bool door_open = g_sim_door_open;

    UI_INFO("[DEBUG] SIM door_open=%d (run_state=%d tick=%p)\n",
            (int)door_open, (int)g_run_state, g_countdown_tick);

    // If door opens while running -> force WAIT immediately + stop oven (safety mock)
    if (door_open && g_run_state == RunState::RUNNING) {
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

static void icon_link_synced(lv_obj_t *link_icon) {
    if (!link_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(link_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(link_icon, ui_color_from_hex(UI_COLOR_LINK_SYNCED), LV_PART_MAIN);
}

static void icon_link_unsynced(lv_obj_t *link_icon) {
    if (!link_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(link_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(link_icon, ui_color_from_hex(UI_COLOR_DANGER_HEX), LV_PART_MAIN);
}

static void icon_link_unused(lv_obj_t *link_icon) {
    if (!link_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(link_icon, LV_OPA_30, LV_PART_MAIN);
}

static void icon_safety_ok(lv_obj_t *safety_icon) {
    if (!safety_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(safety_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(safety_icon, ui_color_from_hex(UI_COLOR_LINK_SYNCED), LV_PART_MAIN); // GREEN
}

static void icon_safety_active(lv_obj_t *safety_icon) {
    if (!safety_icon) {
        return;
    }
    lv_obj_set_style_image_recolor_opa(safety_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(safety_icon, ui_color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX), LV_PART_MAIN); // RED
}

static void update_post_visuals(const OvenRuntimeState &state) {
    if (!ui.preset_box || !ui.dial || !g_main_line_style_inited) {
        return;
    }

    // Only do work on mode change (prevents unnecessary style churn)
    if (state.mode == g_prev_mode_for_post_visuals) {
        return;
    }
    g_prev_mode_for_post_visuals = state.mode;

    const bool is_post = (state.mode == OvenMode::POST);

    // 1) Preset box background
    const uint32_t preset_bg = is_post ? UI_COLOR_DIAL_FRAME_POST : UI_PRESET_BOX_BG_HEX;
    lv_obj_set_style_bg_color(ui.preset_box, ui_color_from_hex(preset_bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.preset_box, (lv_opa_t)UI_PRESET_BOX_BG_OPA, LV_PART_MAIN);

    // 2) Dial frame arc color (main line)
    // POST -> BLUE, otherwise keep default (here: same as before)
    const uint32_t arc_hex = is_post ? UI_COLOR_DIAL_FRAME_POST : UI_COLOR_DIAL_FRAME;
    lv_style_set_arc_color(&g_main_line_style, ui_color_from_hex(arc_hex));

    // Force refresh so LVGL redraws the arc with new style values
    lv_obj_refresh_style(ui.dial, LV_PART_MAIN, LV_STYLE_PROP_ANY);

    UI_INFO("[UI] POST visuals: mode=%s preset_bg=0x%06lx arc=0x%06lx\n",
            oven_mode_to_str(state.mode),
            (unsigned long)preset_bg,
            (unsigned long)arc_hex);
}

// -----------------------------
// Preset label helpers (Step 2.5.2)
// -----------------------------

static void ui_label_set_singleline_clip(lv_obj_t *lbl) {
    if (!lbl) {
        return;
    }
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP); // one line, hard clip
    lv_obj_set_width(lbl, LV_PCT(100));              // use parent width
}

static void ui_set_preset_name_text(const char *name) {
    if (!ui.label_preset_name) {
        return;
    }

    const char *src = (name && name[0]) ? name : "—";

    // Conservative: word-based truncation with "..."
    // NOTE: Adjust these constexpr values as you like.
    static constexpr int UI_PRESET_NAME_MAX_CHARS = 18;

    static char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", src);

    const int len = (int)std::strlen(buf);
    if (len > UI_PRESET_NAME_MAX_CHARS) {
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
        if (cur + 3 < sizeof(buf)) {
            std::strcat(buf, "...");
        }
    }

    lv_label_set_text(ui.label_preset_name, buf);
}

static void ui_set_preset_id(uint32_t id) {
    if (!ui.label_preset_id) {
        return;
    }

    char filament_buf[16];
    std::snprintf(filament_buf, sizeof(filament_buf), "#%u", (unsigned)id);
    lv_label_set_text(ui.label_preset_id, filament_buf);
}

static const lv_font_t *pick_preset_font_for_width(const char *text, lv_coord_t max_w) {
    if (!text || !*text) {
        return ui_preset_font_l();
    }

    const lv_font_t *fonts[] = {ui_preset_font_l(), ui_preset_font_m(), ui_preset_font_s()};

    for (auto f : fonts) {
        lv_point_t sz;
        lv_txt_get_size(&sz, text, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (sz.x <= max_w) {
            return f;
        }
    }
    return ui_preset_font_s();
}

static void preset_name_apply_fit(lv_obj_t *label, const char *text) {
    if (!label || !text || !text[0]) {
        return;
    }

    lv_obj_t *box = lv_obj_get_parent(label);
    lv_obj_update_layout(box);

    lv_coord_t box_w = lv_obj_get_width(box);
    lv_coord_t pad_l = lv_obj_get_style_pad_left(box, LV_PART_MAIN);
    lv_coord_t pad_r = lv_obj_get_style_pad_right(box, LV_PART_MAIN);
    lv_coord_t border = lv_obj_get_style_border_width(box, LV_PART_MAIN);

    lv_coord_t max_w = box_w - pad_l - pad_r - 2 * border;
    if (max_w < 10) {
        max_w = 10;
    }

    const lv_font_t *font = ui_preset_font_s(); // fallback

    lv_point_t sz;

    lv_txt_get_size(&sz, text, ui_preset_font_l(), 0, 0,
                    LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (sz.x <= max_w) {
        font = ui_preset_font_l();
    } else {
        lv_txt_get_size(&sz, text, ui_preset_font_m(), 0, 0,
                        LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (sz.x <= max_w) {
            font = ui_preset_font_m();
        }
    }

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_text(label, text);
}

static RunState derive_run_state_from_runtime(const OvenRuntimeState &st) {
    if (st.delayStartRuntime.active) {
        return st.delayStartRuntime.paused ? RunState::WAIT : RunState::RUNNING;
    }

    // Single source of truth: st.mode (matches OvenMode enum in oven.h)
    // Expected mapping:
    //   STOPPED  -> OvenMode::STOPPED (0)
    //   RUNNING  -> OvenMode::RUNNING (1)   (example)
    //   WAIT     -> OvenMode::WAITING
    //   POST     -> OvenMode::POST
    //
    // IMPORTANT: Treat POST as "active" so the START button stays STOP
    // until POST is finished.

    if (st.mode == OvenMode::WAITING) {
        return RunState::WAIT;
    }

    if (st.mode == OvenMode::STOPPED) {
        return RunState::STOPPED;
    }

    // RUNNING or POST or any future non-stopped mode
    return RunState::RUNNING;
}

//----------------------------------------------------
//
//----------------------------------------------------
// Public API: create the main screen
lv_obj_t *screen_main_create(lv_obj_t *parent) {
    // // Root object
    // if (ui.root != nullptr)
    // {
    //     UI_INFO("return screen_main_create()\n");
    //     return ui.root;
    // }

    // ui.root = lv_obj_create(nullptr);
    // // ui.root = lv_screen_active();
    // lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);

    if (ui.root) {
        return ui.root; // optional caching, ok
    }
    ui.root = lv_obj_create(parent); // create own screen container as child of app root

    lv_obj_remove_style_all(ui.root);
    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);

    lv_obj_set_style_bg_color(ui.root, ui_color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);

    lv_obj_set_size(ui.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_center(ui.root);

    lv_obj_set_style_bg_color(ui.root, ui_color_from_hex(UI_COLOR_BG_HEX), 0);
    lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);

    lv_style_init(&style_dial_border);
    lv_style_set_border_width(&style_dial_border, 1);
    lv_style_set_border_color(&style_dial_border, lv_color_hex(0xFFFFFF)); // Weißer Rahmen
    lv_style_set_border_opa(&style_dial_border, LV_OPA_COVER);
    lv_style_set_radius(&style_dial_border, 12); // Optional: leicht abgerundet
    lv_style_set_pad_all(&style_dial_border, 0);

    // IMPORTANT: disable scrolling in this container (we only use gestures)
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui.root, LV_DIR_NONE);

    // IMPORTANT: allow gestures to bubble up to the app root (screen_manager)
    lv_obj_add_flag(ui.root, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Create sub sections
    create_top_bar(ui.root);
    create_top_bar2(ui.root);

    create_center_section(ui.root);
    create_page_indicator(ui.root);
    // create_page_indicator(ui.page_indicator_container, ScreenId::SCREEN_MAIN);
    create_bottom_section(ui.root);

    UI_DBG("[screen_main_create] screen-addr: %d\n", ui.root);
    return ui.root;
}

void screen_main_topbar2_show(const char *text,
                              uint32_t text_color_hex,
                              uint32_t bg_color_hex,
                              uint32_t timeout_ms) {
    if (!ui.top_bar2_container || !ui.top_bar2_label) {
        return;
    }

    // Stop previous timeout timer
    if (ui.top_bar2_timer) {
        lv_timer_del(ui.top_bar2_timer);
        ui.top_bar2_timer = nullptr;
    }

    // Empty text => hide label only (frame stays)
    if (!text || !text[0]) {
        lv_obj_add_flag(ui.top_bar2_label, LV_OBJ_FLAG_HIDDEN);
        if (ui.top_bar2_container) {
            lv_obj_set_style_bg_opa(ui.top_bar2_container, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui.top_bar2_container, ui_color_from_hex(0xFFFFFF), LV_PART_MAIN);
        }
        return;
    }

    // Apply background + text color
    lv_obj_set_style_bg_color(ui.top_bar2_container, ui_color_from_hex(bg_color_hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.top_bar2_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui.top_bar2_label, ui_color_from_hex(text_color_hex), LV_PART_MAIN);

    // Set text and show
    lv_label_set_text(ui.top_bar2_label, text);
    lv_obj_clear_flag(ui.top_bar2_label, LV_OBJ_FLAG_HIDDEN);

    // Decide: centered if fits, else scrolling (default LVGL speed)
    lv_obj_update_layout(ui.top_bar2_container);

    const lv_coord_t container_w = lv_obj_get_width(ui.top_bar2_container);
    const lv_coord_t pad_l = lv_obj_get_style_pad_left(ui.top_bar2_container, LV_PART_MAIN);
    const lv_coord_t pad_r = lv_obj_get_style_pad_right(ui.top_bar2_container, LV_PART_MAIN);
    const lv_coord_t border = lv_obj_get_style_border_width(ui.top_bar2_container, LV_PART_MAIN);

    lv_coord_t inner_w = container_w - pad_l - pad_r - 2 * border;
    if (inner_w < 10) {
        inner_w = 10;
    }

    lv_obj_set_width(ui.top_bar2_label, inner_w);

    const lv_font_t *font = (const lv_font_t *)lv_obj_get_style_text_font(ui.top_bar2_label, LV_PART_MAIN);

    lv_point_t sz;
    lv_txt_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    if (sz.x <= inner_w) {
        // short text => centered
        lv_label_set_long_mode(ui.top_bar2_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    } else {
        // long text => scrolling (default speed)
        lv_label_set_long_mode(ui.top_bar2_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    }

    lv_obj_center(ui.top_bar2_label);

    // Timeout: hide text only
    if (timeout_ms > 0) {
        ui.top_bar2_timer = lv_timer_create(top_bar2_hide_cb, timeout_ms, nullptr);
        lv_timer_set_repeat_count(ui.top_bar2_timer, 1);
    }
}

//----------------------------------------------------
//
//----------------------------------------------------
// Public API: runtime update
//
void screen_main_update_runtime(const OvenRuntimeState *state) {
    if (!state) {
        return;
    }

    // --- NEW: keep UI run-state in sync with oven runtime ---
    const RunState derived = derive_run_state_from_runtime(*state);
    if (derived != g_run_state) {
        UI_INFO("[UI] run_state sync: %d -> %d (mode=%s running=%d postRem=%u)\n",
                (int)g_run_state,
                (int)derived,
                oven_mode_to_str(state->mode),
                (int)state->running,
                (unsigned)state->post.secondsRemaining);
        g_run_state = derived;
    }

    static bool last_door_open = false;
    const bool door_open_eff = get_effective_door_open(*state);

    if (!g_runtime_edges_initialized) {
        last_door_open = door_open_eff;
        g_prev_door_open_eff = door_open_eff;
        g_prev_host_overtemp = state->hostOvertempActive;
        g_runtime_edges_initialized = true;
    }

    if (door_open_eff != last_door_open) {
        UI_INFO("[DOOR] state changed: %d -> %d (run_state=%d mode=%s running=%d)\n",
                (int)last_door_open, (int)door_open_eff,
                (int)g_run_state, oven_mode_to_str(state->mode), (int)state->running);

        last_door_open = door_open_eff;

        // Always keep pause button consistent with door
        pause_button_update_enabled_by_door(door_open_eff);

        // If door opens while RUNNING -> force WAIT immediately AND tell oven once
        if (door_open_eff && g_run_state == RunState::RUNNING) {
            // snapshot for later resume (optional, but consistent with your debug toggle)
            g_pre_wait_snapshot = g_last_runtime;
            g_has_pre_wait_snapshot = true;

            countdown_stop_and_set_wait_ui("door opened");

            // IMPORTANT: make host state consistent (send WAIT policy/mask)
            oven_pause_wait();

            g_run_state = RunState::WAIT;
            update_start_button_ui();
        }
    }

    // --------------------------------------------------------
    // T10.1.39b – UI Event: DOOR OPEN / CLOSED (edge based)
    // --------------------------------------------------------
    if (door_open_eff != g_prev_door_open_eff) {
        if (door_open_eff) {
            // Rising edge: DOOR OPEN
            screen_main_topbar2_show(
                "DOOR OPEN",
                0xFFFFFF,                    // white text
                UI_COLOR_ICON_DOOR_OPEN_HEX, // red background
                0                            // no timeout (persistent)
            );
        } else {
            // Falling edge: DOOR CLOSED
            screen_main_topbar2_show(
                "DOOR OK",
                0xFFFFFF,
                UI_COLOR_ICON_DOOR_CLOSED_HEX, // green background
                2000                           // auto-hide
            );
        }

        g_prev_door_open_eff = door_open_eff;
    }

    // --------------------------------------------------------
    // T10.1.39b – UI Event: HOST OVERTEMP LOCK ON / OFF
    // --------------------------------------------------------
    const bool host_ot = state->hostOvertempActive;

    if (host_ot != g_prev_host_overtemp) {
        if (host_ot) {
            // Rising edge: OverTemp lock engaged
            screen_main_topbar2_show(
                "OVERTEMP COOLING",
                0xFFFFFF,
                UI_COLOR_WARNING_HEX, // amber/orange
                0                     // persistent while active
            );
        } else {
            // Falling edge: OverTemp recovered
            screen_main_topbar2_show(
                "TEMP OK",
                0xFFFFFF,
                UI_COLOR_BG_HEX,
                2000);
        }

        g_prev_host_overtemp = host_ot;
    }

    g_last_runtime = *state;
    ui_temp_target_tolerance_c = state->tempToleranceC;

    update_time_ui(*state);
    update_dial_ui(*state);
    update_temp_ui(*state);
    update_actuator_icons(*state);
    update_start_button_ui();
    screen_main_refresh_from_runtime();
    pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
    update_fast_preset_buttons_ui();
    update_post_visuals(*state);
    update_status_icons(*state);
}

// Public API: page indicator update
void screen_main_set_active_page(uint8_t page_index) {
    if (page_index >= UI_PAGE_COUNT) {
        return;
    }

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
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

static void create_top_bar(lv_obj_t *parent) {
    ui.top_bar_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.top_bar_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.top_bar_container, UI_SCREEN_WIDTH, 50);
    lv_obj_align(ui.top_bar_container, LV_ALIGN_TOP_MID, 0, UI_TOP_PADDING);

    lv_obj_set_style_bg_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.top_bar_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.top_bar_container, 0, 0);

    // sync icon
    ui.icon_sync = lv_image_create(ui.top_bar_container);
    lv_image_set_src(ui.icon_sync, &link_wht);
    lv_obj_set_size(ui.icon_sync, 32, 32);

    // safety icon (placeholder: link icon asset)
    ui.icon_safety = lv_image_create(ui.top_bar_container);
    lv_image_set_src(ui.icon_safety, &saftey_protect_wht); // <tbd> gegen echtes icon austauschen
    lv_obj_set_size(ui.icon_safety, 32, 32);
    lv_obj_align(ui.icon_safety, LV_ALIGN_LEFT_MID, 36, 0);
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

void screen_main_topbar2_clear_text(void) {
    if (!ui.top_bar2_container || !ui.top_bar2_label) {
        return;
    }

    // hide text, keep frame visible (per spec)
    lv_label_set_text(ui.top_bar2_label, "");
    lv_obj_add_flag(ui.top_bar2_label, LV_OBJ_FLAG_HIDDEN);

    // subtle frame (transparent background)
    lv_obj_set_style_bg_opa(ui.top_bar2_container, LV_OPA_TRANSP, LV_PART_MAIN);

    if (ui.top_bar2_timer) {
        lv_timer_del(ui.top_bar2_timer);
        ui.top_bar2_timer = nullptr;
    }
}

static void topbar2_timeout_cb(lv_timer_t *t) {
    LV_UNUSED(t);
    screen_main_topbar2_clear_text();
}

static void top_bar2_hide_cb(lv_timer_t *t) {
    LV_UNUSED(t);
    screen_main_topbar2_clear_text();

    // if (!ui.top_bar2_label) {
    //     return;
    // }

    // // Behavior per spec:
    // // - hide text on timeout
    // // - frame stays visible
    // lv_obj_add_flag(ui.top_bar2_label, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_set_style_bg_opa(ui.top_bar2_container, LV_OPA_TRANSP, LV_PART_MAIN); // subtle

    // // One-shot timer cleanup
    // if (ui.top_bar2_timer) {
    //     lv_timer_del(ui.top_bar2_timer);
    //     ui.top_bar2_timer = nullptr;
    // }
}

static void topbar2_apply_scroll_mode_if_needed(void) {
    if (!ui.top_bar2_container || !ui.top_bar2_label) {
        return;
    }

    // Content width = container width - left/right padding - borders
    lv_obj_update_layout(ui.top_bar2_container);

    const lv_coord_t w = lv_obj_get_width(ui.top_bar2_container);
    const lv_coord_t pad_l = lv_obj_get_style_pad_left(ui.top_bar2_container, LV_PART_MAIN);
    const lv_coord_t pad_r = lv_obj_get_style_pad_right(ui.top_bar2_container, LV_PART_MAIN);
    const lv_coord_t border = lv_obj_get_style_border_width(ui.top_bar2_container, LV_PART_MAIN);

    lv_coord_t content_w = w - pad_l - pad_r - 2 * border;
    if (content_w < 10) {
        content_w = 10;
    }

    // Measure label text width (current font)
    const char *txt = lv_label_get_text(ui.top_bar2_label);
    if (!txt) {
        txt = "";
    }

    const lv_font_t *font = (const lv_font_t *)lv_obj_get_style_text_font(ui.top_bar2_label, LV_PART_MAIN);
    if (!font) {
        font = LV_FONT_DEFAULT;
    }

    lv_point_t sz;
    lv_txt_get_size(&sz, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    if (sz.x <= content_w) {
        // Fits -> centered, no scrolling
        lv_label_set_long_mode(ui.top_bar2_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    } else {
        // Long -> marquee
        // lv_label_set_long_mode(ui.top_bar2_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        // lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        // lv_label_set_anim_speed(ui.top_bar2_label, 40); // tuned: readable speed
        lv_label_set_long_mode(ui.top_bar2_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    }
}

// ----------------------------------------------------
// TopBar2: full width (minus side padding), framed area for marquee/status text
// Layout rules:
// - Width: UI_SCREEN_WIDTH - 2*UI_SIDE_PADDING
// - X: UI_SIDE_PADDING
// - Y: directly below TopBar1 with a small gap
// ----------------------------------------------------
static void create_top_bar2(lv_obj_t *parent) {
    static constexpr lv_coord_t TOPBAR2_H = 32;
    static constexpr lv_coord_t GAP_Y = 4;

    // Container (acts as the visible frame)
    ui.top_bar2_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.top_bar2_container, LV_OBJ_FLAG_SCROLLABLE);

    // Full width minus padding
    lv_obj_set_size(ui.top_bar2_container,
                    UI_SCREEN_WIDTH - 2 * UI_SIDE_PADDING,
                    TOPBAR2_H);

    // Place directly below TopBar1 (not relative to center)
    lv_obj_align_to(ui.top_bar2_container,
                    ui.top_bar_container,
                    // LV_ALIGN_OUT_BOTTOM_MID,
                    LV_ALIGN_TOP_MID,
                    0,
                    GAP_Y + 30);

    // Style: rounded frame, visible even when text is hidden
    lv_obj_set_style_radius(ui.top_bar2_container, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.top_bar2_container, LV_OPA_TRANSP, LV_PART_MAIN); // subtle
    lv_obj_set_style_bg_color(ui.top_bar2_container, ui_color_from_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_set_style_border_width(ui.top_bar2_container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(ui.top_bar2_container, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.top_bar2_container, ui_color_from_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_set_style_pad_left(ui.top_bar2_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(ui.top_bar2_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ui.top_bar2_container, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(ui.top_bar2_container, 4, LV_PART_MAIN);

    // IMPORTANT: ensure it sits at the correct x by setting its left padding “anchor”
    // (LV_ALIGN_OUT_BOTTOM_MID centers it horizontally, width is already correct)
    // If you prefer explicit x positioning instead of centering, replace align_to with:
    // lv_obj_set_pos(ui.top_bar2_container, UI_SIDE_PADDING, <computed_y>);

    // Label (marquee text) inside the frame
    ui.top_bar2_label = lv_label_create(ui.top_bar2_container);
    lv_label_set_text(ui.top_bar2_label, "");

    // If text is short -> centered (your requirement)
    lv_obj_set_width(ui.top_bar2_label, LV_PCT(100));
    lv_obj_set_style_text_align(ui.top_bar2_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // Default: hidden text, but frame stays visible (your requirement)
    lv_obj_add_flag(ui.top_bar2_label, LV_OBJ_FLAG_HIDDEN);
}

//----------------------------------------------------
//
//----------------------------------------------------
static void create_center_section(lv_obj_t *parent) {
    ui.center_container = lv_obj_create(parent);
    lv_obj_clear_flag(ui.center_container, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_size(ui.center_container, UI_SCREEN_WIDTH, UI_DIAL_SIZE + 20);
    // lv_obj_align(ui.center_container, LV_ALIGN_CENTER, 0, -10); // slight up offset

    // neu T10.1.39.3
    lv_obj_set_size(ui.center_container, UI_SCREEN_WIDTH, UI_DIAL_SIZE + 20);
    // Place the center strictly below TopBar2 to avoid overlap.
    // This makes the layout deterministic even if TopBar2 height changes.
    static constexpr lv_coord_t GAP_Y = 4;
    lv_obj_align_to(ui.center_container,
                    ui.top_bar2_container,
                    LV_ALIGN_OUT_BOTTOM_MID,
                    0,
                    GAP_Y);

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
    // static lv_style_t main_line_style;
    // lv_style_init(&main_line_style);
    // lv_style_set_arc_color(&main_line_style, /*lv_color_black()*/ ui_color_from_hex(UI_COLOR_DIAL_FRAME));
    // lv_style_set_arc_width(&main_line_style, 8);
    // lv_obj_add_style(ui.dial, &main_line_style, LV_PART_MAIN);

    // /* Main line properties */
    if (!g_main_line_style_inited) {
        lv_style_init(&g_main_line_style);
        g_main_line_style_inited = true;
    }

    // Default color at boot (normal mode)
    lv_style_set_arc_color(&g_main_line_style, ui_color_from_hex(UI_COLOR_DIAL_FRAME));
    lv_style_set_arc_width(&g_main_line_style, 8);

    lv_obj_add_style(ui.dial, &g_main_line_style, LV_PART_MAIN);

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
    static constexpr lv_coord_t kRightButtonW = UI_SIDE_PADDING;
    static constexpr lv_coord_t kRightButtonGap = 8;
    static constexpr lv_coord_t kRightButtonGroupGap = kRightButtonGap * 2;
    static constexpr lv_coord_t kRightButtonH =
        (UI_DIAL_SIZE - (4 * kRightButtonGap) - kRightButtonGroupGap) / 6;
    static constexpr lv_coord_t kFastPresetButtonH = kRightButtonH - 4;

    ui.start_button_container = lv_obj_create(ui.center_container);
    lv_obj_clear_flag(ui.start_button_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui.start_button_container, kRightButtonW, UI_DIAL_SIZE);
    lv_obj_align(ui.start_button_container, LV_ALIGN_RIGHT_MID, -UI_FRAME_PADDING, 0);
    lv_obj_set_style_bg_opa(ui.start_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.start_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.start_button_container, 0, 0);

    ui.btn_start = lv_btn_create(ui.start_button_container);
    lv_obj_set_size(ui.btn_start, kRightButtonW, kRightButtonH);
    lv_obj_align(ui.btn_start, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(ui.btn_start, start_button_event_cb, LV_EVENT_CLICKED, nullptr);

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
    lv_obj_set_size(ui.btn_pause, kRightButtonW, kRightButtonH);
    lv_obj_align(ui.btn_pause, LV_ALIGN_TOP_MID, 0, kRightButtonH + kRightButtonGap);

    lv_obj_set_style_radius(ui.btn_pause, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.btn_pause, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.btn_pause, ui_color_from_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.btn_pause, LV_OPA_COVER, LV_PART_MAIN);

    ui.label_btn_pause = lv_label_create(ui.btn_pause);
    lv_label_set_text(ui.label_btn_pause, "PAUSE");
    lv_obj_center(ui.label_btn_pause);
    lv_obj_add_event_cb(ui.btn_pause, pause_button_event_cb, LV_EVENT_CLICKED, nullptr);

    for (uint16_t i = 0; i < kFastPresetSlotCount; ++i) {
        const lv_coord_t y = (2 * kRightButtonH) + kRightButtonGap + kRightButtonGroupGap - 4 + (i * (kFastPresetButtonH + kRightButtonGap));
        ui.btn_fast_preset[i] = lv_btn_create(ui.start_button_container);
        lv_obj_set_size(ui.btn_fast_preset[i], kRightButtonW, kFastPresetButtonH);
        lv_obj_align(ui.btn_fast_preset[i], LV_ALIGN_TOP_MID, 0, y);
        lv_obj_set_style_radius(ui.btn_fast_preset[i], 8, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(ui.btn_fast_preset[i], LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui.btn_fast_preset[i], ui_color_from_hex(UI_COL_FAST_PRESET_BG_HEX), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_fast_preset[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui.btn_fast_preset[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.btn_fast_preset[i], ui_color_from_hex(UI_COL_FAST_PRESET_BORDER_HEX), LV_PART_MAIN);
        lv_obj_set_style_border_opa(ui.btn_fast_preset[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_event_cb(ui.btn_fast_preset[i], fast_preset_button_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        ui.label_fast_preset[i] = lv_label_create(ui.btn_fast_preset[i]);
        char label_buf[8];
        fast_preset_label_text(s_fast_preset_ids[i], label_buf, sizeof(label_buf));
        lv_label_set_text(ui.label_fast_preset[i], label_buf);
        lv_obj_set_style_text_color(ui.label_fast_preset[i], ui_color_from_hex(UI_COL_FAST_PRESET_TEXT_HEX), LV_PART_MAIN);
        lv_obj_center(ui.label_fast_preset[i]);
    }

    ui_set_pause_enabled(false);
    update_fast_preset_buttons_ui();

    UI_INFO("Needles: MM=%p HH=%p SS=%p\n", ui.needleMM, ui.needleHH, ui.needleSS);
    UI_INFO("MM p0=%d,%d p1=%d,%d\n",
            (int)g_minute_hand_points[0].x, (int)g_minute_hand_points[0].y,
            (int)g_minute_hand_points[1].x, (int)g_minute_hand_points[1].y);
}

//----------------------------------------------------
// Page-Indicator section
//----------------------------------------------------
static void create_page_indicator(lv_obj_t *parent) {
    ui.page_indicator_container = lv_obj_create(parent);
    lv_obj_set_size(ui.page_indicator_container, UI_SCREEN_WIDTH, UI_PAGE_INDICATOR_HEIGHT);
    lv_obj_align_to(ui.page_indicator_container, ui.center_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(ui.page_indicator_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.page_indicator_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.page_indicator_container, 0, 0);

    // Inner panel (rounded rectangle)
    // T7
    ui.page_indicator_panel = lv_obj_create(ui.page_indicator_container);
    lv_obj_set_size(ui.page_indicator_panel, 130, 24); // width will auto-fit for 3 dots
    lv_obj_center(ui.page_indicator_panel);
    lv_obj_clear_flag(ui.page_indicator_panel, LV_OBJ_FLAG_SCROLLABLE);

    // after creating ui.page_indicator_panel
    lv_obj_add_flag(ui.page_indicator_panel, LV_OBJ_FLAG_CLICKABLE);

    // at the end:
    ui.s_swipe_target = ui.page_indicator_panel;

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

    for (uint8_t i = 0; i < UI_PAGE_COUNT; ++i) {
        ui.page_dots[i] = lv_obj_create(ui.page_indicator_panel);
        lv_obj_set_size(ui.page_dots[i], UI_PAGE_DOT_SIZE, UI_PAGE_DOT_SIZE);
        lv_obj_clear_flag(ui.page_dots[i], LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_opa(ui.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ui.page_dots[i], UI_PAGE_DOT_SIZE / 2, 0);
        lv_obj_set_style_border_opa(ui.page_dots[i], LV_OPA_TRANSP, 0);

        lv_obj_set_style_margin_left(ui.page_dots[i], (i == 0) ? 0 : UI_PAGE_DOT_SPACING, 0);

        lv_color_t col = (i == UI_PAGE_MAIN)
                             ? ui_color_from_hex(UI_COLOR_PAGE_DOT_ACTIVE_HEX)
                             : ui_color_from_hex(UI_COLOR_PAGE_DOT_INACTIVE_HEX);
        lv_obj_set_style_bg_color(ui.page_dots[i], col, 0);
    }

    // Swipe hit area (wide, invisible, click target)
    ui.swipe_hit = lv_obj_create(ui.page_indicator_container);
    lv_obj_remove_style_all(ui.swipe_hit);
    lv_obj_set_size(ui.swipe_hit, 360, UI_PAGE_INDICATOR_HEIGHT); // middle width
    lv_obj_align(ui.swipe_hit, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(ui.swipe_hit, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_style_bg_opa(ui.swipe_hit, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(ui.swipe_hit, LV_OBJ_FLAG_CLICKABLE);
    // Very subtle swipe hint background
    lv_obj_set_style_bg_color(ui.swipe_hit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui.swipe_hit, 15, 0); //
    lv_obj_set_style_radius(ui.swipe_hit, 8, 0);
    // Expose swipe target to screen_manager
    ui.s_swipe_target = ui.swipe_hit;
}

//----------------------------------------------------
// Bottom section creation (temperature scale)
//----------------------------------------------------
static void create_bottom_section(lv_obj_t *parent) {
    static constexpr lv_coord_t kBottomH = 58;
    static constexpr lv_coord_t kBarW = UI_TEMP_SCALE_WIDTH;
    static constexpr lv_coord_t kBarH = 16;
    static constexpr lv_coord_t kCurrentBarY = 22;
    static constexpr lv_coord_t kTargetBarY = 39;

    ui.bottom_container = lv_obj_create(parent);
    lv_obj_set_size(ui.bottom_container, UI_SCREEN_WIDTH, kBottomH);
    lv_obj_align(ui.bottom_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(ui.bottom_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ui.bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui.bottom_container, 0, 0);

    ui.temp_scale_current = lv_bar_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_scale_current, kBarW, kBarH);
    lv_obj_align(ui.temp_scale_current, LV_ALIGN_TOP_MID, 0, kCurrentBarY);
    lv_obj_clear_flag(ui.temp_scale_current, LV_OBJ_FLAG_SCROLLABLE);
    lv_bar_set_range(ui.temp_scale_current, UI_TEMP_MIN_C, UI_TEMP_MAX_C);
    lv_bar_set_value(ui.temp_scale_current, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.temp_scale_current, ui_color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.temp_scale_current, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.temp_scale_current, ui_color_from_hex(UI_COLOR_TEMP_CURRENT_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.temp_scale_current, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui.temp_scale_current, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.temp_scale_current, 6, LV_PART_INDICATOR);

    ui.temp_scale_target = lv_bar_create(ui.bottom_container);
    lv_obj_set_size(ui.temp_scale_target, kBarW, kBarH);
    lv_obj_align(ui.temp_scale_target, LV_ALIGN_TOP_MID, 0, kTargetBarY);
    lv_obj_clear_flag(ui.temp_scale_target, LV_OBJ_FLAG_SCROLLABLE);
    lv_bar_set_range(ui.temp_scale_target, UI_TEMP_MIN_C, UI_TEMP_MAX_C);
    lv_bar_set_value(ui.temp_scale_target, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.temp_scale_target, ui_color_from_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.temp_scale_target, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.temp_scale_target, ui_color_from_hex(UI_COLOR_TEMP_TARGET_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.temp_scale_target, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui.temp_scale_target, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.temp_scale_target, 6, LV_PART_INDICATOR);

    // --------------------------------------------------------
    // Temperature labels
    // --------------------------------------------------------
    ui.temp_label_current = lv_label_create(ui.bottom_container);
    lv_obj_set_style_text_color(ui.temp_label_current, lv_color_white(), 0);
    lv_label_set_text(ui.temp_label_current, "-- °C");
    lv_obj_add_flag(ui.temp_label_current, LV_OBJ_FLAG_IGNORE_LAYOUT);

    ui.temp_label_hotspot = lv_label_create(ui.bottom_container);
    lv_obj_set_style_text_color(ui.temp_label_hotspot, ui_color_from_hex(UI_COLOR_TEMP_HOTSPOT_HEX), 0);
    lv_label_set_text(ui.temp_label_hotspot, "HS: -- °C");
    lv_obj_add_flag(ui.temp_label_hotspot, LV_OBJ_FLAG_IGNORE_LAYOUT);

    ui.temp_label_target = lv_label_create(ui.bottom_container);
    lv_obj_set_style_text_color(ui.temp_label_target, lv_color_white(), 0);
    lv_label_set_text(ui.temp_label_target, "-- °C");
    lv_obj_add_flag(ui.temp_label_target, LV_OBJ_FLAG_IGNORE_LAYOUT);

    ui.temp_tri_hotspot = lv_image_create(ui.bottom_container);
    lv_image_set_src(ui.temp_tri_hotspot, &temp_tri_down_wht);
    lv_obj_set_size(ui.temp_tri_hotspot, UI_TEMP_TRI_W, UI_TEMP_TRI_H);
    lv_obj_add_flag(ui.temp_tri_hotspot, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_hotspot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(ui.temp_tri_hotspot, ui_color_from_hex(UI_COLOR_TEMP_HOTSPOT_HEX), LV_PART_MAIN);

    ui.temp_tol_low_line = lv_obj_create(ui.bottom_container);
    lv_obj_remove_style_all(ui.temp_tol_low_line);
    lv_obj_set_size(ui.temp_tol_low_line, 2, kBarH + 4);
    lv_obj_add_flag(ui.temp_tol_low_line, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_color(ui.temp_tol_low_line, ui_color_from_hex(UI_COLOR_TEMP_BAND_HEX), 0);
    lv_obj_set_style_bg_opa(ui.temp_tol_low_line, LV_OPA_COVER, 0);

    ui.temp_tol_high_line = lv_obj_create(ui.bottom_container);
    lv_obj_remove_style_all(ui.temp_tol_high_line);
    lv_obj_set_size(ui.temp_tol_high_line, 2, kBarH + 4);
    lv_obj_add_flag(ui.temp_tol_high_line, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_color(ui.temp_tol_high_line, ui_color_from_hex(UI_COLOR_TEMP_BAND_HEX), 0);
    lv_obj_set_style_bg_opa(ui.temp_tol_high_line, LV_OPA_COVER, 0);
}

//
// Runtime updates
//

// ----------------------------------------------------
// - recolor special icons
// -    * link
// -    * .
// ----------------------------------------------------
static void update_status_icons(const OvenRuntimeState &state) {
    // 1) Link icon recolor
    if (state.linkSynced) {
        icon_link_synced(ui.icon_sync); // GREEN
    } else {
        icon_link_unsynced(ui.icon_sync); // RED
    }
    // 1b) Safety icon recolor (T13)
    if (state.safetyCutoffActive) {
        icon_safety_active(ui.icon_safety); // RED
    } else {
        icon_safety_ok(ui.icon_safety); // GREEN
    }

    // 2) TopBar2 status line (priority-based, edge-triggered)
    enum class UiStatus : uint8_t {
        NONE = 0,
        COMM_DOWN,
        LINK_UNSYNCED,
        DOOR_OPEN,
        OVERTEMP,
        WAITING,
        POST_ACTIVE
    };

    auto pick_status = [&](const OvenRuntimeState &s) -> UiStatus {
        // Highest priority first
        if (!s.commAlive) {
            return UiStatus::COMM_DOWN;
        }
        if (!s.linkSynced) {
            return UiStatus::LINK_UNSYNCED;
        }
        if (get_effective_door_open(s)) {
            return UiStatus::DOOR_OPEN;
        }
        if (s.hostOvertempActive) {
            return UiStatus::OVERTEMP;
        }
        if (s.mode == OvenMode::WAITING) {
            return UiStatus::WAITING;
        }
        if (s.mode == OvenMode::POST) {
            return UiStatus::POST_ACTIVE;
        }
        return UiStatus::NONE;
    };

    static UiStatus s_prev = UiStatus::NONE;
    const UiStatus cur = pick_status(state);

    if (!g_status_banner_initialized) {
        s_prev = cur;
        g_status_banner_initialized = true;
        if (cur == UiStatus::NONE) {
            screen_main_topbar2_clear_text();
        }
        return;
    }

    if (cur == s_prev) {
        return; // no change -> avoid flicker/spam
    }
    s_prev = cur;

    // Apply on change
    switch (cur) {
    case UiStatus::COMM_DOWN:
        screen_main_topbar2_show("COMM LOST", UI_COLOR_WHITE_FG_HEX, UI_COLOR_DANGER_HEX, 0);
        break;

    case UiStatus::LINK_UNSYNCED:
        screen_main_topbar2_show("LINK UNSYNCED", UI_COLOR_WHITE_FG_HEX, UI_COLOR_DANGER_HEX, 0);
        break;

    case UiStatus::DOOR_OPEN:
        screen_main_topbar2_show("DOOR OPEN", UI_COLOR_WHITE_FG_HEX, UI_COLOR_DANGER_HEX, 0);
        break;

    case UiStatus::OVERTEMP:
        screen_main_topbar2_show("OVER TEMP - COOLING", UI_COLOR_WHITE_FG_HEX, UI_COLOR_COOLDOWN_HEX, 0);
        break;

    case UiStatus::WAITING:
        screen_main_topbar2_show("WAIT", UI_COLOR_WHITE_FG_HEX, UI_COLOR_WARNING_HEX, 0);
        break;

    case UiStatus::POST_ACTIVE:
        screen_main_topbar2_show("POST COOLING", UI_COLOR_WHITE_FG_HEX, UI_COLOR_COOLDOWN_HEX, 0);
        break;

    case UiStatus::NONE:
    default:
        screen_main_topbar2_clear_text();
        break;
    }
}
// static void update_status_icons(const OvenRuntimeState &state) {
//     if (state.linkSynced) {
//         icon_link_synced(ui.icon_sync); // GREEN
//     } else {
//         icon_link_unsynced(ui.icon_sync); // RED
//     }
// }

//----------------------------------------------------
// update_time
// aktualisiert die Heiz-Zeit HH:MM:SS
// berechnet Restzeit
//----------------------------------------------------
static void update_time_ui(const OvenRuntimeState &state) {
    const bool delay_waiting = state.delayStartRuntime.active && state.delayStartRuntime.waiting;
    const uint32_t totalSeconds = delay_waiting ? oven_get_delay_start_seconds()
                                                : state.durationMinutes * 60;
    const uint32_t remaining = delay_waiting ? state.delayStartRuntime.delayRemainingSec
                                             : state.secondsRemaining;

    // UI countdown owns the top bar while running (prevents flicker with oven runtime updates)
    if (g_countdown_tick != nullptr) {
        return;
    }

    if (totalSeconds > 0) {
        uint32_t elapsed = (remaining <= totalSeconds) ? (totalSeconds - remaining) : totalSeconds;
        lv_bar_set_range(ui.time_bar, 0, totalSeconds);
        lv_bar_set_value(ui.time_bar, elapsed, LV_ANIM_OFF);
    } else {
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

static void update_dial_ui(const OvenRuntimeState &state) {
    // Ensure sizes/styles are resolved before measuring
    lv_obj_update_layout(ui.preset_box);

    // Compute available width inside the preset box (content area)
    lv_coord_t box_w = lv_obj_get_width(ui.preset_box);

    lv_coord_t pad_l = lv_obj_get_style_pad_left(ui.preset_box, LV_PART_MAIN);
    lv_coord_t pad_r = lv_obj_get_style_pad_right(ui.preset_box, LV_PART_MAIN);
    lv_coord_t border_w = lv_obj_get_style_border_width(ui.preset_box, LV_PART_MAIN);

    lv_coord_t max_text_w = box_w - pad_l - pad_r - 2 * border_w;

    // Safety clamp
    if (max_text_w < 10) {
        max_text_w = 10;
    }

    // Preset name (top line)
    if (ui.label_preset_name) {
        const bool delay_waiting = state.delayStartRuntime.active && state.delayStartRuntime.waiting;
        const char *name = delay_waiting ? "TIMER" : state.presetName;
        lv_label_set_text(ui.label_preset_name, name);

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
    if (state.delayStartRuntime.active && state.delayStartRuntime.waiting) {
        std::snprintf(filament_buf, sizeof(filament_buf), "%s",
                      state.delayStartRuntime.paused ? "paused" : "waiting");
    } else {
        std::snprintf(filament_buf, sizeof(filament_buf), "#%u", (unsigned)state.filamentId);
    }
    if (ui.label_preset_id) {
        lv_label_set_text(ui.label_preset_id, filament_buf);
    }
}

//----------------------------------------------------
// update_temp
// update der aktuellen Temperatur-Labels
// update der Scale-für IST-Temperatur
//----------------------------------------------------
static void update_temp_ui(const OvenRuntimeState &state) {
    if (!ui.temp_scale_current || !ui.temp_scale_target) {
        return;
    }

    float cur_f = state.tempCurrent;
    float tgt_f = state.tempTarget;
    float hot_f = state.tempHotspotC;

    int16_t cur = (int16_t)lroundf(cur_f);
    int16_t tgt = (int16_t)lroundf(tgt_f);
    int16_t hot = (int16_t)lroundf(hot_f);
    int16_t tol = (int16_t)lroundf(state.tempToleranceC);

    if (cur < UI_TEMP_MIN_C) {
        cur = UI_TEMP_MIN_C;
    }
    if (cur > UI_TEMP_MAX_C) {
        cur = UI_TEMP_MAX_C;
    }
    if (tgt < UI_TEMP_MIN_C) {
        tgt = UI_TEMP_MIN_C;
    }
    if (tgt > UI_TEMP_MAX_C) {
        tgt = UI_TEMP_MAX_C;
    }
    if (hot < UI_TEMP_MIN_C) {
        hot = UI_TEMP_MIN_C;
    }
    if (hot > UI_TEMP_MAX_C) {
        hot = UI_TEMP_MAX_C;
    }

    lv_bar_set_value(ui.temp_scale_current, cur, LV_ANIM_OFF);
    lv_bar_set_value(ui.temp_scale_target, tgt, LV_ANIM_OFF);

    char buf_cur[16];
    std::snprintf(buf_cur, sizeof(buf_cur), "%d °C", (int)cur);
    lv_label_set_text(ui.temp_label_current, buf_cur);

    char buf_tgt[16];
    std::snprintf(buf_tgt, sizeof(buf_tgt), "%d °C", (int)tgt);
    lv_label_set_text(ui.temp_label_target, buf_tgt);

    if (state.tempHotspotValid) {
        char buf_hot[16];
        std::snprintf(buf_hot, sizeof(buf_hot), "HS: %d °C", (int)hot);
        lv_label_set_text(ui.temp_label_hotspot, buf_hot);
        lv_obj_clear_flag(ui.temp_tri_hotspot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui.temp_label_hotspot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(ui.temp_label_hotspot, "HS: -- °C");
        lv_obj_add_flag(ui.temp_tri_hotspot, LV_OBJ_FLAG_HIDDEN);
    }

    const bool in_range = (cur >= (tgt - tol)) && (cur <= (tgt + tol));
    lv_obj_set_style_text_opa(ui.temp_label_target,
                              in_range ? LV_OPA_70 : LV_OPA_COVER,
                              LV_PART_MAIN);

    lv_obj_set_style_img_recolor(ui.temp_tri_hotspot, ui_color_from_hex(UI_COLOR_TEMP_HOTSPOT_HEX), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui.temp_tri_hotspot, LV_OPA_COVER, LV_PART_MAIN);

    lv_coord_t current_scale_x = lv_obj_get_x(ui.temp_scale_current);
    lv_coord_t current_scale_y = lv_obj_get_y(ui.temp_scale_current);
    lv_coord_t current_scale_w = lv_obj_get_width(ui.temp_scale_current);
    lv_coord_t current_scale_h = lv_obj_get_height(ui.temp_scale_current);

    lv_coord_t target_scale_x = lv_obj_get_x(ui.temp_scale_target);
    lv_coord_t target_scale_y = lv_obj_get_y(ui.temp_scale_target);
    lv_coord_t target_scale_w = lv_obj_get_width(ui.temp_scale_target);
    lv_coord_t target_scale_h = lv_obj_get_height(ui.temp_scale_target);

    auto value_to_x = [&](int16_t value) -> lv_coord_t {
        float t = (float)(value - UI_TEMP_MIN_C) / (float)(UI_TEMP_MAX_C - UI_TEMP_MIN_C);
        if (t < 0.0f) {
            t = 0.0f;
        }
        if (t > 1.0f) {
            t = 1.0f;
        }
        return (lv_coord_t)(t * (float)current_scale_w);
    };

    lv_coord_t cur_x = current_scale_x + value_to_x(cur);
    lv_coord_t hot_x = current_scale_x + value_to_x(hot);
    lv_coord_t tgt_x = target_scale_x + value_to_x(tgt);
    const int16_t low = LV_MAX((int16_t)UI_TEMP_MIN_C, (int16_t)(tgt - tol));
    const int16_t high = LV_MIN((int16_t)UI_TEMP_MAX_C, (int16_t)(tgt + tol));
    lv_coord_t low_x = target_scale_x + value_to_x(low);
    lv_coord_t high_x = target_scale_x + value_to_x(high);

    lv_coord_t hot_tri_x = hot_x - (UI_TEMP_TRI_W / 2);

    lv_coord_t hot_tri_y = current_scale_y - UI_TEMP_TRI_H - 1;

    lv_obj_set_pos(ui.temp_tri_hotspot, hot_tri_x, hot_tri_y);

    lv_obj_update_layout(ui.temp_label_current);
    lv_coord_t cur_label_w = lv_obj_get_width(ui.temp_label_current);
    lv_coord_t cur_label_h = lv_obj_get_height(ui.temp_label_current);
    lv_coord_t cur_label_x = cur_x - (cur_label_w / 2);
    if (cur_label_x < current_scale_x + 4) {
        cur_label_x = current_scale_x + 4;
    }
    if (cur_label_x + cur_label_w > current_scale_x + current_scale_w - 4) {
        cur_label_x = current_scale_x + current_scale_w - cur_label_w - 4;
    }
    lv_obj_set_pos(ui.temp_label_current,
                   cur_label_x,
                   current_scale_y + ((current_scale_h - cur_label_h) / 2));

    lv_obj_update_layout(ui.temp_label_target);
    lv_coord_t tgt_label_w = lv_obj_get_width(ui.temp_label_target);
    lv_coord_t tgt_label_x = tgt_x - (tgt_label_w / 2);
    if (tgt_label_x < target_scale_x + 4) {
        tgt_label_x = target_scale_x + 4;
    }
    if (tgt_label_x + tgt_label_w > target_scale_x + target_scale_w - 4) {
        tgt_label_x = target_scale_x + target_scale_w - tgt_label_w - 4;
    }
    lv_obj_set_pos(ui.temp_label_target,
                   tgt_label_x,
                   target_scale_y + ((target_scale_h - lv_obj_get_height(ui.temp_label_target)) / 2));

    lv_obj_update_layout(ui.temp_label_hotspot);
    lv_coord_t hot_label_w = lv_obj_get_width(ui.temp_label_hotspot);
    lv_coord_t hot_label_x = hot_tri_x - hot_label_w - 6;
    if (hot_label_x < current_scale_x + 4) {
        hot_label_x = current_scale_x + 4;
    }
    if (hot_label_x + hot_label_w > hot_tri_x - 2) {
        hot_label_x = hot_tri_x - hot_label_w - 2;
    }
    lv_obj_set_pos(ui.temp_label_hotspot,
                   hot_label_x,
                   hot_tri_y + ((UI_TEMP_TRI_H - lv_obj_get_height(ui.temp_label_hotspot)) / 2));

    lv_obj_set_pos(ui.temp_tol_low_line,
                   low_x - 1,
                   target_scale_y);
    lv_obj_set_pos(ui.temp_tol_high_line,
                   high_x - 1,
                   target_scale_y);
}

//----------------------------------------------------
// update_actuator_icons
// führt update für Icons for (Farbwechsel)
// Icons signalisieren lediglich ON/OFF
//
// NOTE:
// RUNNING and POST intentionally share the same icon behavior.
// POST is visually indicated by dial + preset background only.
//----------------------------------------------------
static void update_actuator_icons(const OvenRuntimeState &state) {
    auto set_icon_state = [](lv_obj_t *obj, lv_color_t on_color, bool on) {
        if (!obj) {
            return;
        }

        if (on) {
            // Recolor the white icon to the given ON color
            lv_obj_set_style_img_recolor(obj, on_color, LV_PART_MAIN);
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            // OFF: show original white icon (no recolor)
            lv_obj_set_style_img_recolor_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
        }
    };
    // Door icon is always live from real runtime state
    const bool door_open = get_effective_door_open(state);
    const lv_color_t col_on = ui_color_from_hex(UI_COLOR_ICON_DOOR_CLOSED_HEX);      // z.B. grün
    const lv_color_t col_door_open = ui_color_from_hex(UI_COLOR_ICON_DOOR_OPEN_HEX); // z.B. rot

    // Door icon: always show state (closed=green, open=red), never "white"
    if (ui.icon_door) {
        lv_obj_set_style_img_recolor(ui.icon_door, door_open ? col_door_open : col_on, LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(ui.icon_door, LV_OPA_COVER, LV_PART_MAIN);
    }

    // Heater icon is intentionally binary:
    // OFF = white, ON = green. No temperature-dependent blue state.
    set_icon_state(ui.icon_heater, col_on, state.heater_on);

    // WAIT override: show safe-state regardless of the real actuator bits
    if (g_run_state == RunState::WAIT) {
        set_icon_state(ui.icon_fan230, col_on, false);     // OFF
        set_icon_state(ui.icon_fan230_slow, col_on, true); // ON
        set_icon_state(ui.icon_fan12v, col_on, true);      // ON
        set_icon_state(ui.icon_lamp, col_on, true);        // ON
        set_icon_state(ui.icon_heater, col_on, false);     // OFF
        set_icon_state(ui.icon_motor, col_on, false);      // OFF
        return;
    }
    // ------------------------------------------------------------
    // T10.1.39c: Host OverTemp visual override (RUNNING only)
    // - Ensure icons reflect the intended safety airflow immediately:
    //   FAN230V=ON, FAN230V_SLOW=OFF
    // ------------------------------------------------------------

    // // 12V fan: white (off) -> green (on)
    // set_icon_state(ui.icon_fan12v, col_on, state.fan12v_on);

    // // 230V fan fast: white (off) -> green (on)
    // set_icon_state(ui.icon_fan230, col_on, state.fan230_on);

    // // 230V fan slow: white (off) -> green (on)
    // set_icon_state(ui.icon_fan230_slow, col_on, state.fan230_slow_on);

    if (state.hostOvertempActive && (state.mode == OvenMode::RUNNING)) {
        set_icon_state(ui.icon_fan12v, col_on, state.fan12v_on);
        set_icon_state(ui.icon_fan230, col_on, true);       // FAST ON
        set_icon_state(ui.icon_fan230_slow, col_on, false); // SLOW OFF

        // Continue updating the remaining icons normally below
    } else {
        // 12V fan normal
        set_icon_state(ui.icon_fan12v, col_on, state.fan12v_on);

        // Fans normal
        set_icon_state(ui.icon_fan230, col_on, state.fan230_on);
        set_icon_state(ui.icon_fan230_slow, col_on, state.fan230_slow_on);
    }

    // Heater: white (off) -> green (on)
    // set_icon_state(ui.icon_heater, col_on, state.heater_on);

    // Motor: white (off) -> green (on)
    set_icon_state(ui.icon_motor, col_on, state.motor_on);

    // Lamp: white (off) -> green (on)
    set_icon_state(ui.icon_lamp, col_on, state.lamp_on);
}

static void update_start_button_ui(void) {
    if (!ui.btn_start || !ui.label_btn_start) {
        return;
    }

    const bool running = (g_run_state != RunState::STOPPED);

    if (running) {
        // Oven is running: button should be red and show STOP
        lv_label_set_text(ui.label_btn_start, "STOP");
        lv_obj_set_style_bg_color(ui.btn_start, ui_color_from_hex(UI_COLOR_DANGER_HEX), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.btn_start, LV_OPA_COVER, LV_PART_MAIN);
    } else {
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
static void start_button_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    lv_event_code_t code = lv_event_get_code(e);

    // T10.1.36b (Schritt 2)
    // Block START if door is open (safety)
    const bool door_open = get_effective_door_open(g_last_runtime);
    if (g_run_state == RunState::STOPPED && door_open) {
        UI_INFO("[START] blocked: door open\n");
        return;
    }

    // Wichtig: irgendein Log, das du sicher siehst
    UI_INFO("start_button_event_cb(): code=%d\n", (int)code);

    if (g_run_state != RunState::STOPPED) {
        // STOP from RUNNING or WAIT
        oven_stop();

        if (g_countdown_tick) {
            lv_timer_del(g_countdown_tick);
            g_countdown_tick = nullptr;
        }

        g_run_state = RunState::STOPPED;
        update_fast_preset_buttons_ui();
        UI_INFO("OVEN_STOPPED\n");
        return;
    }
    // START from STOPPED
    if (!oven_start_or_schedule()) {
        UI_WARN("[START] start_or_schedule rejected\n");
        return;
    }

    OvenRuntimeState st{};
    oven_get_runtime_state(&st);
    g_run_state = derive_run_state_from_runtime(st);
    UI_INFO("OVEN_STARTED_OR_SCHEDULED\n");
    screen_main_refresh_from_runtime();

    if (g_remaining_seconds <= 0) {
        UI_INFO("[COUNTDOWN] nothing to start (remaining_seconds=%d)\n", g_remaining_seconds);
        return;
    }

    // if (g_countdown_tick) {
    //     lv_timer_del(g_countdown_tick);
    //}

    g_total_seconds = g_remaining_seconds;

    lv_bar_set_range(ui.time_bar, 0, g_total_seconds);
    lv_bar_set_value(ui.time_bar, 0, LV_ANIM_OFF);

    set_remaining_label_seconds(g_remaining_seconds);

    // deprecated: oven_tick ist ausschließlich für die Zeit zuständig
    // g_countdown_tick = lv_timer_create(countdown_tick_cb, COUNT_TICK_UPDATE_FREQ, &ui);
    pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
    update_fast_preset_buttons_ui();
    UI_INFO("[COUNTDOWN] started: %d seconds\n", g_remaining_seconds);
}

static void pause_button_event_cb(lv_event_t *e) {
    LV_UNUSED(e);

    if (g_last_runtime.delayStartRuntime.active && g_last_runtime.delayStartRuntime.waiting) {
        if (g_last_runtime.delayStartRuntime.paused) {
            if (!oven_delay_start_resume()) {
                return;
            }
            g_run_state = RunState::RUNNING;
        } else {
            if (!oven_delay_start_pause()) {
                return;
            }
            g_run_state = RunState::WAIT;
        }

        pause_button_apply_ui(g_run_state, false);
        update_start_button_ui();
        update_fast_preset_buttons_ui();
        return;
    }

    // erweitert in T10.1.36b (Schritt 2)
    if (g_run_state == RunState::RUNNING) {
        if (get_effective_door_open(g_last_runtime)) {
            UI_INFO("[WAIT] cannot enter WAIT: door open (already unsafe)\n");
            // Optional: wir sind faktisch schon unsafe, aber Client killt; hier keine Aktion
            return;
        }

        // Enter WAIT: stop countdown timer, snapshot runtime
        if (g_countdown_tick) {
            lv_timer_del(g_countdown_tick);
            g_countdown_tick = nullptr;
        }

        g_pre_wait_snapshot = g_last_runtime;
        g_has_pre_wait_snapshot = true;

        // IMPORTANT: actually request WAIT in oven (policy mask)
        oven_pause_wait();

        g_run_state = RunState::WAIT;

        pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
        update_start_button_ui();
        update_fast_preset_buttons_ui();

        UI_INFO("[WAIT] (pause_button_event_cb) entered\n");
        return;
    }

    // if (g_run_state == RunState::RUNNING) {
    //     // Enter WAIT: stop countdown timer, snapshot runtime
    //     if (g_countdown_tick) {
    //         lv_timer_del(g_countdown_tick);
    //         g_countdown_tick = nullptr;
    //     }

    //     g_pre_wait_snapshot = g_last_runtime;
    //     g_has_pre_wait_snapshot = true;

    //     g_run_state = RunState::WAIT;

    //     UI_INFO("[WAIT] (pause_button_event_cb) entered\n");
    //     return;
    // }

    if (g_run_state == RunState::WAIT) {
        if (get_effective_door_open(g_last_runtime)) {
            UI_INFO("[WAIT] cannot resume: door open\n");
            return;
        }

        if (!oven_resume_from_wait()) {
            UI_INFO("[WAIT] resume rejected by oven\n");
            return;
        }

        g_run_state = RunState::RUNNING;
        pause_button_apply_ui(g_run_state, get_effective_door_open(g_last_runtime));
        update_start_button_ui();
        update_fast_preset_buttons_ui();

        UI_INFO("[WAIT] resumed\n");
        return;
    }
}

static void fast_preset_button_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (g_run_state != RunState::STOPPED) {
        UI_INFO("[FAST_PRESET] blocked: run_state=%d\n", (int)g_run_state);
        return;
    }

    const uintptr_t slot = (uintptr_t)lv_event_get_user_data(e);
    if (slot >= kFastPresetSlotCount) {
        return;
    }

    const uint16_t preset_id = s_fast_preset_ids[slot];
    UI_INFO("[FAST_PRESET] slot=%u preset=%u\n", (unsigned)slot, (unsigned)preset_id);

    oven_select_preset(preset_id);

    OvenRuntimeState st{};
    oven_get_runtime_state(&st);
    screen_main_update_runtime(&st);
}

lv_obj_t *screen_main_get_swipe_target(void) {
    return ui.s_swipe_target;
}

// END OF FILE
