# T15 HeaterCurve Algorithm – Current Design Description

## Purpose

This document describes the current **HeaterCurve control concept** for the T15 test endpoint.

The goal is to create a control strategy that later can be reused in **T16 / host-side oven control** and that keeps the **chamber temperature** close to the configured target while avoiding overshoot.

The current target behavior is:

- hold chamber temperature around `SIM_TARGET_TEMP`
- allow at most `SIM_TARGET_MAX_OVERSHOT` above target
- allow at most `SIM_TARGET_MIN_UNDERSHOT` below target
- always prioritize safety over heating performance

---

## System Context

T15 is a **client-side laboratory environment**.

It is **not** the final production architecture.

The final production architecture remains:

- **Host / oven.cpp** = Single Source of Truth
- **Client** = actuator execution + telemetry + safety enforcement

T15 exists only to experimentally determine a good heater control strategy under real hardware conditions.

---

## Available Inputs

The current HeaterCurve concept uses these inputs:

### 1. Chamber temperature
- Source: ADS1115 channel A1
- NTC type: 10k
- Meaning: **main control temperature**
- This is the temperature that should be held close to the target

### 2. Hotspot temperature
- Source: ADS1115 channel A0
- NTC type: 100k
- Meaning: **fast / local heater-area indicator**
- This is not the main target temperature, but it helps detect stored heat energy and avoid overshoot

### 3. Door state
- `DOOR_OPEN = HIGH (5V)`
- `DOOR_CLOSED = LOW (GND)`

This is a **hard safety gate**:

- Door open -> heater must be OFF
- Door closed -> heater may run

### 4. Time
- `millis()` based timing
- used for:
  - sampling
  - control tick
  - slope estimation
  - hold windows / duty windows

---

## Available Outputs

The controller currently produces:

### Heater duty percent
A requested heater energy value in percent:

- `0%` = heater off
- `100%` = full heating request

The client then maps this to real hardware actuation:

- PWM on heater output
- 4 kHz
- robust attach / detach logic
- safe level when disabled

---

## Safety Philosophy

Safety is intentionally stronger than control performance.

### Hard safety conditions
The heater must be forced OFF if one of these happens:

1. door is open
2. chamber sensor is invalid
3. hotspot sensor exceeds a hard abort temperature
4. chamber exceeds a hard abort temperature
5. any future safety condition added later

### Safety latch concept
The current T15 concept uses a **latched safe-off state**.

That means:

- once a severe safety condition is detected
- heater is turned off
- the system remains in safe-off until reboot or explicit reset logic

This mirrors the general T14/T15 safety philosophy.

---

## Core Overshoot Problem

The overshoot problem comes from the physical system behavior:

1. The heater injects energy very quickly
2. The hotspot sensor reacts quickly
3. The chamber reacts slowly
4. If we heat until chamber is already near target, there is still stored heat in the system
5. That stored heat causes chamber temperature to continue rising after heater off

This is the exact reason why a simple thermostat / naive hysteresis often overshoots.

---

## Current Overshoot Avoidance Strategy

The current HeaterCurve design avoids overshoot using **three mechanisms**:

### 1. Band-based target logic
The target is not treated as a single point.

Instead we define a band:

- lower band = `target - minUndershoot`
- upper band = `target + maxOvershoot`

This means:
- below lower band -> more heating allowed
- inside band -> maintain carefully
- above upper band -> heater off

This already prevents “always chase exact target” behavior.

---

### 2. Chamber slope estimation
The controller estimates how fast the chamber temperature is currently rising.

This is done by:

- comparing current chamber temperature to previous chamber temperature
- dividing by elapsed time
- filtering the result with a low-pass filter

This produces an estimate like:

- positive slope -> chamber is warming up
- negative slope -> chamber is cooling down
- near zero -> chamber is stable

Why this matters:

If the chamber is already rising quickly, then continuing to heat is dangerous, because the system still contains stored energy.

---

### 3. Predicted chamber temperature
The controller computes a simple prediction:

`predicted chamber = current chamber + (slope * tau)`

Where:
- `slope` = filtered chamber temperature rise rate
- `tau` = expected thermal lag / remaining rise time

Meaning:

If the chamber is currently still below the target but the predicted value would exceed the upper band, then the heater is turned off **early**.

This is the key anti-overshoot mechanism.

It is intentionally simple and explainable.

---

## Role of Hotspot Temperature

Hotspot is used as a fast “energy already in the heater area” indicator.

### Why hotspot matters
The chamber is slow.
The hotspot reacts much faster.

So even if chamber still looks safe, a very high hotspot means:

- energy is currently being pumped in strongly
- chamber may rise later
- overshoot risk is increasing

### Current hotspot logic
The controller uses hotspot as a **soft cap**:

- if hotspot exceeds an absolute limit -> heating request reduced or disabled
- if `(hotspot - chamber)` becomes too large -> heating request reduced or disabled

This delta is important because it measures how much thermal imbalance exists between heater area and chamber.

A large delta means:
- the heater area is already much hotter than the chamber
- chamber will likely continue rising even after heater off

So hotspot helps us switch off **before** chamber overshoots.

---

## Controller Modes

The current control concept uses a small number of modes.

### SAFE_OFF
Used when:
- door open
- invalid sensor
- abort condition
- future safety condition

Behavior:
- heater request = 0%

---

### HEAT_UP
Used when chamber is still clearly below the allowed band.

Behavior:
- strong heating request
- but still limited by hotspot safety / hotspot delta cap

Purpose:
- get from cold system into usable range quickly

---

### APPROACH
Used when chamber is approaching the target region.

Behavior:
- reduce heater request
- check predicted chamber temperature
- stop early if prediction says overshoot is likely

Purpose:
- transition from “fast heating” to “controlled arrival”

This is the most important overshoot-avoidance mode.

---

### HOLD
Used once chamber is inside the allowed temperature band.

Behavior:
- maintain temperature using reduced energy
- no aggressive full-power heating
- use a windowed duty concept for gentler energy injection

Purpose:
- prevent oscillation
- hold chamber around target without repeated large overshoot

---

## Why Windowed Duty Is Used In HOLD

Instead of constantly pushing heater power in a naive way, HOLD mode uses a time window.

Example:
- hold window = 2000 ms
- requested duty = 25%
- heater active only during part of that window

This has advantages:

1. less thermal shock
2. easier observation in logs
3. less oscillation than simple on/off threshold chatter
4. more deterministic than arbitrary pulse hacks

---

## How We Will Evaluate the HeaterCurve

We will validate the algorithm using **real hardware** and **UDP logs**.

### What will be observed
For every run we want to record:

- door state
- chamber temperature
- hotspot temperature
- heater state / duty
- controller mode
- chamber slope
- predicted chamber temperature
- safety reason bits
- time

### What success means
The algorithm is considered good when:

1. chamber reaches target reliably
2. chamber does not overshoot more than allowed
3. chamber remains in the acceptable band for a long time
4. hotspot does not run away dangerously
5. door opening always forces heater off immediately
6. behavior is understandable from logs

---

## Why This Is Better Than Simple Hysteresis

A simple hysteresis controller only looks at current temperature.

That is not enough here, because the system has thermal lag.

This HeaterCurve concept adds:

- a target band
- slope estimation
- future prediction
- hotspot delta awareness
- hard safety gating

So it reacts not only to where the chamber **is now**, but also to where it is likely to go next.

That is the main reason overshoot should be reduced significantly.

---

## Current Limitations

This is still an experimental control concept.

Current limitations:

1. tuning constants are still empirical
2. exact hotspot thresholds still need real-world verification
3. slope filtering may need adjustment
4. prediction tau may need adjustment
5. hold duty behavior still needs tuning against real chamber inertia

---

## Planned Next Refinements

The next refinement steps are expected to be:

1. integrate controller output into live client run
2. log all controller internals
3. run controlled heating experiments
4. tune:
   - prediction tau
   - hotspot delta cap
   - hold duty window
   - approach aggressiveness
5. move the validated algorithm later into host-side control for T16

---

## Summary

The HeaterCurve concept tries to avoid overshoot by combining:

- hard safety gating
- chamber band control
- chamber slope estimation
- simple forward prediction
- hotspot-based early cutoff
- gentle hold behavior

In simple words:

- heat strongly when cold
- reduce power before target
- switch off early if the future looks too hot
- hold gently once inside the allowed band
- safety always wins
