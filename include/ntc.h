#pragma once

/*
===============================================================================
ntc.h — NTC / ADC utility helpers (lookup curve + filtering + hysteresis)
===============================================================================

What this header provides
-------------------------
This header is intentionally "small and dependency-free" and provides:

1) A linear lookup table to convert an ADC measurement to a temperature value:
   - Input:  adc_counts (int32)
   - Output: temperature in deci-°C (int32), e.g. 253 => 25.3°C

2) Simple, deterministic filtering helpers for noisy ADC signals:
   - median5(): median filter for 5 samples (excellent for spike removal)
   - MovingAverage<N>: moving average (excellent for smoothing)

3) A small hysteresis switch for on/off control (avoids rapid toggling):
   - HysteresisSwitch::update(temp_dC, setpoint_dC, hyst_dC)

The code is designed to be usable on microcontrollers without heap allocation,
and most helpers are constexpr / inline where possible.

Important: Temperature representation (deci-°C)
-----------------------------------------------
All temperature values in this header use "deci-degree Celsius" (dC):

  200  => 20.0°C
  253  => 25.3°C
  1100 => 110.0°C

This provides stable integer math and avoids floating point in most places.

How the NTC curve conversion works
----------------------------------
The function ntc_adc_to_temp_dC(adc_counts) uses a lookup table:

  kAdcAtTemp[]  <->  kTemp_dC[]

Both arrays must have the same length N and represent paired points:
  kAdcAtTemp[i] is the ADC count measured at temperature kTemp_dC[i].

Conversion is done via piecewise linear interpolation between the nearest
two points.

Monotonic requirement:
----------------------
kAdcAtTemp[] MUST be monotonic:
- either strictly increasing with temperature,
- or strictly decreasing with temperature.

This is important so lookup_linear_i32() can safely find the correct segment.

Where do adc_counts come from?
------------------------------
In your project, adc_counts typically come from ADS1115 readings, e.g.:

  int16_t raw = ads.readADC_SingleEnded(channel);

You can feed that raw value directly into ntc_adc_to_temp_dC(), *as long as*
the kAdcAtTemp[] table was built for exactly that same measurement setup:
- same ADS gain setting
- same channel
- same NTC divider wiring
- same supply/reference conditions

If you change ADS gain or wiring, you must rebuild the table.

===============================================================================
NOTE ABOUT THESE TWO PLACEHOLDER ARRAYS
===============================================================================
The following arrays appear to be older placeholders and are currently not used
by any code in this header. They are left here for compatibility, but they
should not be relied on as-is (all zeros). Prefer the kTemp_dC/kAdcAtTemp table.

If you want, these can be removed once your project is cleaned up.
*/

// static constexpr int ntc_temp_curve_degree[] = {20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
// static constexpr float ntc_temp_curve_voltage[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

#include <stddef.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Integer linear interpolation helpers
// -----------------------------------------------------------------------------

/**
 * @brief Integer linear interpolation with rounding.
 *
 * Interpolates y(x) between the points (x0,y0) and (x1,y1).
 *
 * - Uses integer math only.
 * - Includes rounding to nearest integer.
 * - Handles x0==x1 safely (returns y0).
 *
 * @param x   Query position
 * @param x0  Segment start x
 * @param y0  Segment start y
 * @param x1  Segment end x
 * @param y1  Segment end y
 * @return Interpolated y value (rounded)
 */
static constexpr int32_t lerp_i32(int32_t x, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x1 == x0) {
        return y0;
    }
    const int32_t num = (x - x0) * (y1 - y0);
    const int32_t den = (x1 - x0);

    // Rounded division:
    // Add half denominator (with sign) before dividing.
    return y0 + (num + (den > 0 ? den / 2 : -den / 2)) / den;
}

/**
 * @brief Piecewise linear lookup with clamping (integer).
 *
 * Given monotonic x_points[] and corresponding y_points[],
 * this function:
 * - clamps to endpoints for out-of-range x,
 * - finds the segment where x lies,
 * - returns a linearly interpolated y (rounded).
 *
 * Monotonicity:
 * - x_points[] may be increasing or decreasing.
 * - The function detects direction automatically by comparing the first segment.
 *
 * @tparam N number of points (must be >= 2)
 * @param x query x
 * @param x_points monotonic x lookup points
 * @param y_points corresponding y values
 * @return interpolated y
 */
template <size_t N>
static constexpr int32_t lookup_linear_i32(int32_t x, const int32_t (&x_points)[N], const int32_t (&y_points)[N]) {
    static_assert(N >= 2, "Need >= 2 points");

    const bool inc = (x_points[1] >= x_points[0]);

    // Clamp to range
    if (inc) {
        if (x <= x_points[0]) {
            return y_points[0];
        }
        if (x >= x_points[N - 1]) {
            return y_points[N - 1];
        }
    } else {
        if (x >= x_points[0]) {
            return y_points[0];
        }
        if (x <= x_points[N - 1]) {
            return y_points[N - 1];
        }
    }

    // Find segment and interpolate
    for (size_t i = 0; i < N - 1; ++i) {
        const int32_t x0 = x_points[i], x1 = x_points[i + 1];
        const bool in_seg = inc ? (x >= x0 && x <= x1) : (x <= x0 && x >= x1);
        if (!in_seg) {
            continue;
        }
        return lerp_i32(x, x0, y_points[i], x1, y_points[i + 1]);
    }

    // Should not happen if monotonic, but return last as a safe fallback.
    return y_points[N - 1];
}

// -----------------------------------------------------------------------------
// Hysteresis helper (for stable ON/OFF control decisions)
// -----------------------------------------------------------------------------

/**
 * @brief Simple hysteresis state machine for ON/OFF switching.
 *
 * Typical use case:
 * - Decide whether a heater output should be ON or OFF based on current temperature.
 * - Prevent rapid toggling around the setpoint by requiring the temperature to
 *   move outside a band before changing the state.
 *
 * Example:
 *   HysteresisSwitch heaterSw;
 *   bool heaterOn = heaterSw.update(temp_dC, setpoint_dC, 20); // ±2.0°C band
 *
 * Semantics:
 * - Turns ON only when temp <= (setpoint - hyst)
 * - Turns OFF only when temp >= (setpoint + hyst)
 */
struct HysteresisSwitch {
    bool state = false; // current output state (e.g. heater ON/OFF)

    /**
     * @param temp_dC     current temperature in deci-°C
     * @param setpoint_dC target temperature in deci-°C
     * @param hyst_dC     hysteresis half-band in deci-°C (±), e.g. 20 => ±2.0°C
     * @return resulting state after hysteresis logic
     */
    bool update(int32_t temp_dC, int32_t setpoint_dC, int32_t hyst_dC) {
        const int32_t low = setpoint_dC - hyst_dC;
        const int32_t high = setpoint_dC + hyst_dC;

        if (!state && temp_dC <= low) {
            state = true;
        }
        if (state && temp_dC >= high) {
            state = false;
        }
        return state;
    }
};

// -----------------------------------------------------------------------------
// Median filter (5 samples) — robust spike remover
// -----------------------------------------------------------------------------

/**
 * @brief Median of 5 values using a fixed sorting network (no loops).
 *
 * Median filtering is excellent for removing occasional spikes/glitches
 * that can occur on ADC lines.
 *
 * Typical pattern:
 *   int32_t s0 = read();
 *   int32_t s1 = read();
 *   int32_t s2 = read();
 *   int32_t s3 = read();
 *   int32_t s4 = read();
 *   int32_t adc_med = median5(s0,s1,s2,s3,s4);
 *
 * @return the median value (middle of sorted 5)
 */
static inline int32_t median5(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    // Sorting network for 5 elements
    if (a > b) {
        int32_t t = a;
        a = b;
        b = t;
    }
    if (c > d) {
        int32_t t = c;
        c = d;
        d = t;
    }
    if (a > c) {
        int32_t t = a;
        a = c;
        c = t;
    }
    if (b > d) {
        int32_t t = b;
        b = d;
        d = t;
    }
    if (b > c) {
        int32_t t = b;
        b = c;
        c = t;
    }
    if (e < b) { return b; }
    if (e > c) { return c; }
    return e;
}

// -----------------------------------------------------------------------------
// Moving average (N samples) — smoothing for stable readings
// -----------------------------------------------------------------------------

/**
 * @brief Moving average filter with fixed window size.
 *
 * Keeps the last N samples in a ring buffer and returns the rounded average.
 *
 * Notes:
 * - Uses int64 accumulator to avoid overflow.
 * - Before the buffer is filled, it averages only the samples collected so far.
 *
 * Example:
 *   MovingAverage<16> avg;
 *   int32_t adc_smooth = avg.update(adc_raw);
 */
template <size_t N>
struct MovingAverage {
    static_assert(N >= 2, "N must be >= 2");

    int32_t buf[N] = {};
    int64_t sum = 0;
    size_t idx = 0;
    bool filled = false;

    /**
     * @param v new sample
     * @return rounded average of window (or partial window during startup)
     */
    int32_t update(int32_t v) {
        sum -= buf[idx];
        buf[idx] = v;
        sum += v;

        idx++;
        if (idx >= N) {
            idx = 0;
            filled = true;
        }

        const size_t div = filled ? N : idx;
        return (int32_t)((sum + (int64_t)div / 2) / (int64_t)div);
    }
};

// -----------------------------------------------------------------------------
// NTC curve table (ADC counts -> temperature in deci-°C)
// -----------------------------------------------------------------------------

/*
Your curve is defined here using paired arrays:

  kTemp_dC[i]    = temperature at point i (deci-°C)
  kAdcAtTemp[i]  = ADC count measured at that temperature

You must fill kAdcAtTemp[] with real values.

How to build kAdcAtTemp[] in practice
-------------------------------------
Option A (empirical calibration, recommended):
1) Stabilize the sensor at each temperature point (e.g. using a reference thermometer).
2) Read many ADC samples for each point (e.g. 100..500) and average.
3) Store the averaged ADC value into kAdcAtTemp[i].

Option B (calculated, if you know exact divider + NTC curve):
- Compute expected voltage at each temperature.
- Convert voltage to ADS1115 raw codes given the gain.
- Still verify empirically because real-world tolerances matter.

Monotonic check:
- After filling kAdcAtTemp[], confirm it is monotonic.
  If not monotonic, interpolation will behave incorrectly.

Important for ADS1115:
- The ADS gain setting changes scaling and potentially raw ranges.
- Keep gain constant when calibrating and when running.

Schematic note:
- You stated your internal NTC is connected to ADS1115 ADC0 (AIN0).
- Ensure you are using the same channel consistently.
*/

// Temperature points in deci-°C: 200 = 20.0°C
// static constexpr int32_t kTemp_dC[11] = {200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200};
static constexpr int32_t kTemp_dC[11] = {0, 210, 310, 800, 1200};

// Fill with ADC counts measured/calculated at those temperatures.
// Must be monotonic (either increasing or decreasing).
// static constexpr int32_t kAdcAtTemp[11] = {0, 20, 30, 0, 0, 0, 0, 0, 0, 0, 0};
static constexpr int32_t kAdcAtTemp[11] = {
    5000, // every temperature below 20° are not interested
    2700, // approx 21°
    2350,
    2000,
    1500};

/**
 * @brief Convert ADC counts (from ADS1115 / ADC path) to temperature (deci-°C).
 *
 * This performs a clamped, piecewise-linear interpolation using the curve table.
 *
 * @param adc_counts raw ADC reading (must match calibration setup)
 * @return temperature in deci-°C
 */
static inline int32_t ntc_adc_to_temp_dC(int32_t adc_counts) {
    return lookup_linear_i32(adc_counts, kAdcAtTemp, kTemp_dC);
}

/*
===============================================================================
USAGE EXAMPLES
===============================================================================

1) Minimal: ADC -> temp_dC
--------------------------------
int16_t adc = ads.readADC_SingleEnded(0);     // if your NTC is on ADS AIN0
int32_t temp_dC = ntc_adc_to_temp_dC(adc);    // e.g. 253 => 25.3°C

2) With spike removal: median of 5
----------------------------------
int16_t s0 = ads.readADC_SingleEnded(0);
int16_t s1 = ads.readADC_SingleEnded(0);
int16_t s2 = ads.readADC_SingleEnded(0);
int16_t s3 = ads.readADC_SingleEnded(0);
int16_t s4 = ads.readADC_SingleEnded(0);

int32_t adc_med = median5(s0,s1,s2,s3,s4);
int32_t temp_dC = ntc_adc_to_temp_dC(adc_med);

3) With smoothing: moving average
---------------------------------
MovingAverage<16> avg;
int16_t adc = ads.readADC_SingleEnded(0);
int32_t adc_smooth = avg.update(adc);
int32_t temp_dC = ntc_adc_to_temp_dC(adc_smooth);

4) Combined: median(5) + moving average
---------------------------------------
MovingAverage<16> avg;

int16_t s0 = ads.readADC_SingleEnded(0);
int16_t s1 = ads.readADC_SingleEnded(0);
int16_t s2 = ads.readADC_SingleEnded(0);
int16_t s3 = ads.readADC_SingleEnded(0);
int16_t s4 = ads.readADC_SingleEnded(0);

int32_t adc_med = median5(s0,s1,s2,s3,s4);
int32_t adc_filt = avg.update(adc_med);

int32_t temp_dC = ntc_adc_to_temp_dC(adc_filt);

5) Heater ON/OFF control with hysteresis
----------------------------------------
HysteresisSwitch heaterSw;

int32_t temp_dC = ...;             // current temperature
int32_t setpoint_dC = 600;         // 60.0°C
int32_t hyst_dC = 20;              // ±2.0°C

bool heaterOn = heaterSw.update(temp_dC, setpoint_dC, hyst_dC);
// heaterOn can be used to set an output bit or to enable PWM, etc.

===============================================================================
*/