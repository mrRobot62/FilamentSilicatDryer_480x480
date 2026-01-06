#include "screen_base.h"

// ------------------------------------------------------------
// Debug frames for base layout containers
// Enable via compiler define:
//   -DUI_BASE_DEBUG_FRAMES=1
// ------------------------------------------------------------
#ifndef UI_BASE_DEBUG_FRAMES
#define UI_BASE_DEBUG_FRAMES 1
#endif

#if UI_BASE_DEBUG_FRAMES
static inline void apply_debug_frame(lv_obj_t *obj, lv_color_t c) {
    // 1px border + rounded corners
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, c, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_50, 0);

    // Rounded corners (tweak if you want)
    lv_obj_set_style_radius(obj, 8, 0);

    // Optional: slightly visible background to see extents
    // Comment out if you only want border.
    lv_obj_set_style_bg_opa(obj, LV_OPA_10, 0);

    // No padding side effects
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static inline void apply_debug_outline_frame(lv_obj_t *obj, lv_color_t c) {
    // Use OUTLINE so children can't paint over it
    lv_obj_set_style_outline_width(obj, 1, 0);
    lv_obj_set_style_outline_color(obj, c, 0);
    lv_obj_set_style_outline_opa(obj, LV_OPA_100, 0);
    lv_obj_set_style_outline_pad(obj, 0, 0);

    // Rounded corners (visual only)
    lv_obj_set_style_radius(obj, 8, 0);

    // Optional: small bg to see extents
    // lv_obj_set_style_bg_opa(obj, LV_OPA_10, 0);
}
#else
static inline void apply_debug_frame(lv_obj_t *, lv_color_t) {}
static inline void apply_debug_outline_frame(lv_obj_t *obj, lv_color_t c) {}
#endif

/*
 * Apply the default style for the screen root.
 *
 * Responsibilities:
 * - Full black background (no transparency)
 * - No padding (pixel-perfect layout control)
 * - Non-scrollable (root must never scroll)
 *
 * This is the visual and structural root for every screen.
 */
static void style_root_black(lv_obj_t *obj) {
    lv_obj_remove_style_all(obj);

    // Solid black background
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);

    // No implicit spacing
    lv_obj_set_style_pad_all(obj, 0, 0);

    // Root must not scroll
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/*
 * Apply a fully transparent, non-interfering container style.
 *
 * Used for all structural layout containers:
 * - top bar
 * - bottom bar
 * - center area
 * - side containers
 *
 * These containers exist purely for geometry and alignment.
 */
static void style_transparent(lv_obj_t *obj) {
    lv_obj_remove_style_all(obj);

    // Fully transparent background
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);

    // No padding to avoid layout side effects
    lv_obj_set_style_pad_all(obj, 0, 0);

    // Containers must not scroll by default
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/*
 * Create the base screen layout.
 *
 * This function defines the immutable screen structure shared by
 * all screens (main, config, log, etc.).
 *
 * Layout overview (top → bottom):
 *
 * ┌──────────────────────────────┐
 * │ TOP BAR                      │  height = top_h
 * ├──────────────────────────────┤
 * │ CENTER                       │
 * │ ┌────┬──────────┬───────┐    │
 * │ │LEFT│  MIDDLE  │ RIGHT │    │  height = center_h
 * │ └────┴──────────┴───────┘    │
 * ├──────────────────────────────┤
 * │ PAGE INDICATOR / SWIPE ZONE  │  height = page_h
 * ├──────────────────────────────┤
 * │ BOTTOM BAR                   │  height = bottom_h
 * └──────────────────────────────┘
 *
 * All sizing is explicit and deterministic.
 */
void screen_base_create(ScreenBaseLayout *out, lv_obj_t *parent,
                        int screen_w, int screen_h,
                        int top_h, int page_h, int bottom_h,
                        int side_w, int center_w) {
    if (!out) {
        return;
    }

    /* ------------------------------------------------------------
     * ROOT CONTAINER
     * ------------------------------------------------------------ */
    out->root = lv_obj_create(parent);
    style_root_black(out->root);

    // Root always matches full screen resolution
    lv_obj_set_size(out->root, screen_w, screen_h);
    lv_obj_center(out->root);
    // apply_debug_frame(out->root, lv_color_hex(0xFFFFFF)); // white

    /* ------------------------------------------------------------
     * TOP BAR
     * ------------------------------------------------------------ */
    out->top = lv_obj_create(out->root);
    style_transparent(out->top);

    // Fixed height top bar, full width
    lv_obj_set_size(out->top, screen_w, top_h);
    lv_obj_align(out->top, LV_ALIGN_TOP_MID, 0, 0);
    // apply_debug_frame(out->top, lv_color_hex(0xFFFFFF)); // white

    /* ------------------------------------------------------------
     * BOTTOM BAR
     * ------------------------------------------------------------ */
    out->bottom = lv_obj_create(out->root);
    style_transparent(out->bottom);

    // Fixed height bottom bar, full width
    lv_obj_set_size(out->bottom, screen_w, bottom_h);
    lv_obj_align(out->bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    // apply_debug_frame(out->bottom, lv_color_hex(0xFFFFFF)); // white

    /* ------------------------------------------------------------
     * PAGE INDICATOR / SWIPE ZONE
     * ------------------------------------------------------------ */
    out->page_indicator = lv_obj_create(out->root);
    style_transparent(out->page_indicator);

    /*
     * This container serves two purposes:
     * - Visual page indicator (dots, hints)
     * - Swipe gesture capture area
     *
     * It is positioned directly above the bottom bar.
     */
    lv_obj_set_size(out->page_indicator, center_w, page_h);
    lv_obj_align(out->page_indicator, LV_ALIGN_BOTTOM_MID, 0, -bottom_h);
    // apply_debug_frame(out->page_indicator, lv_color_hex(0xFFFFFF)); // white

    /* ------------------------------------------------------------
     * CENTER AREA (main content region)
     * ------------------------------------------------------------ */
    const int center_h = screen_h - top_h - page_h - bottom_h;

    out->center = lv_obj_create(out->root);
    style_transparent(out->center);

    // Center spans full width and remaining height
    lv_obj_set_size(out->center, screen_w, center_h);
    lv_obj_align(out->center, LV_ALIGN_TOP_MID, 0, top_h);
    // apply_debug_frame(out->center, lv_color_hex(0xFFFFFF)); // white

    /* ------------------------------------------------------------
     * LEFT CONTAINER (icons / status indicators)
     * ------------------------------------------------------------ */
    out->left = lv_obj_create(out->center);
    style_transparent(out->left);

    // Fixed-width side container, full center height
    lv_obj_set_size(out->left, side_w, center_h);
    lv_obj_align(out->left, LV_ALIGN_LEFT_MID, 0, 0);
    // #26ff00ff
    // apply_debug_frame(out->left, lv_color_hex(0x26ff00)); // white

    /* ------------------------------------------------------------
     * MIDDLE CONTAINER (primary screen content)
     * ------------------------------------------------------------ */
    out->middle = lv_obj_create(out->center);
    style_transparent(out->middle);

    /*
     * Middle container hosts:
     * - Cards
     * - Rollers
     * - Main UI widgets
     *
     * Width is explicit to avoid flex side effects.
     */
    lv_obj_set_size(out->middle, center_w, center_h);
    lv_obj_align(out->middle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(out->middle, 5, 0);

    // #00bbffff
    // apply_debug_frame(out->middle, lv_color_hex(0x00bbff));
    //apply_debug_outline_frame(out->middle, ui_color_from_hex(0xffffff));
    apply_debug_frame(out->middle, ui_color_from_hex(0xffffff));
    /* ------------------------------------------------------------
     * RIGHT CONTAINER (buttons / actions)
     * ------------------------------------------------------------ */
    out->right = lv_obj_create(out->center);
    style_transparent(out->right);

    // Symmetric to left container
    lv_obj_set_size(out->right, side_w, center_h);
    lv_obj_align(out->right, LV_ALIGN_RIGHT_MID, 0, 0);
    // #ff00c8
    // apply_debug_frame(out->right, lv_color_hex(0xff00c8)); // white
}