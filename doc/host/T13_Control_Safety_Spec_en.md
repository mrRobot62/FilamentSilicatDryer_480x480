# T13 Control & Safety Specification

**Project:** FilamentSilicatDryer_480x480\
**Architecture Version:** T13 (Dual‑NTC, Relay‑friendly Pulse Control)\
**Date:** 2026‑02‑19

------------------------------------------------------------------------

# Change‑Log

## T13 (2026‑02‑19)

-   Introduced Dual‑NTC architecture:
    -   `tempHotspot` (Safety & thermal dynamics monitoring)
    -   `tempChamber` (Primary control & UI temperature)
-   Removed PT1/Bias thermal model completely.
-   Introduced preset‑based chamber limits (`chamberMaxC`).
-   Introduced relay‑friendly adaptive pulse scheduler.
-   Introduced multi‑layer safety model:
    -   Chamber preset cap
    -   Chamber hard cap
    -   Hotspot soft and hard caps
    -   Sensor fail‑safe handling

------------------------------------------------------------------------

# System Overview

The system consists of:

-   Host: ESP32‑S3 (UI, control logic, safety logic)
-   Client: ESP32‑WROOM (sensor acquisition, actuator control)
-   Temperature sensors: Two independent NTC sensors connected via
    ADS1115 ADC

Temperature roles:

  Sensor        Purpose
  ------------- -----------------------------------------------
  tempChamber   Primary control temperature
  tempHotspot   Safety monitoring and dynamic heating limiter

The Host is the **single source of truth** for all control and safety
decisions.

------------------------------------------------------------------------

# Sensor Acquisition (Client)

ADC: ADS1115\
Gain: ±6.144 V\
Resolution: 16‑bit signed integer

Mapping:

  ADS Channel   Purpose
  ------------- -------------
  CH0           tempHotspot
  CH1           tempChamber
  CH2           Reserved
  CH3           Reserved

RAW values transmitted unmodified.

Temperature transmission unit:

-   deci‑degrees Celsius (°C × 10)
-   Example: 509 = 50.9°C

------------------------------------------------------------------------

# STATUS Protocol Frame

Format:

    C;STATUS;<mask_hex>;<adc0>;<adc1>;<adc2>;<adc3>;<tempHotspot_dC>;<tempChamber_dC>

Example:

    C;STATUS;0035;12345;11890;0;0;509;372

------------------------------------------------------------------------

# Preset Model

Each preset defines:

  Field         Description
  ------------- -------------------------------------
  targetC       Control target temperature
  toleranceC    Hysteresis tolerance
  chamberMaxC   Maximum allowed chamber temperature

Example (Silicagel preset):

  Field         Value
  ------------- -------
  targetC       105.0
  toleranceC    2.0
  chamberMaxC   110.0

Control band:

    lo = targetC − toleranceC
    hi = targetC + toleranceC

Example:

    103°C … 107°C

------------------------------------------------------------------------

# Safety Model

Safety layers (highest priority first):

## Layer 1 --- Chamber Hard Limit

    CHAMBER_HARD_MAX_C = 120°C

If exceeded:

    Heater OFF immediately

## Layer 2 --- Preset Chamber Limit

    capC = min(chamberMaxC, CHAMBER_HARD_MAX_C)

If exceeded:

    Heater OFF

## Layer 3 --- Hotspot Safety

Relative thresholds:

    HOTSPOT_SOFT_MAX_C = capC + 15°C
    HOTSPOT_HARD_MAX_C = capC + 30°C

Behavior:

  Condition    Action
  ------------ ------------------------
  ≥ soft max   Increase REST duration
  ≥ hard max   Heater OFF

Global clamp:

    HOTSPOT_HARD_MAX_C ≤ 200°C

------------------------------------------------------------------------

# Pulse Scheduler (Relay‑Friendly Control)

Relay switching constraints require slow switching.

Default timing parameters:

    HEAT_MS_MIN = 2000
    HEAT_MS_MAX = 8000

    REST_MS_MIN = 6000
    REST_MS_MAX = 60000

    MIN_TOGGLE_MS = 2000

Near‑target behavior:

    nearBandC = 3.0°C
    restNearTargetMinMs = 20000

Behavior:

  Condition              Result
  ---------------------- -----------------------------
  Far from target        Moderate heat pulses
  Near target            Minimum heat, extended rest
  Fast hotspot rise      Extended rest
  Safety limit reached   Heater OFF

------------------------------------------------------------------------

# Fail‑Safe Handling

Invalid temperature sentinel:

    temp = -32768

If detected:

    Heater OFF
    Fault state optional

------------------------------------------------------------------------

# Control Authority

Control decisions use exclusively:

    tempChamberC

Hotspot temperature is used only for:

-   Safety enforcement
-   Adaptive timing adjustment

UI displays:

    tempChamberC

------------------------------------------------------------------------

# Example: Silicagel Operation

Preset:

    target = 105°C
    tolerance = 2°C
    max = 110°C

Result:

Stable operating range:

    100°C … 110°C

Overshoot prevented by:

-   chamberMaxC
-   pulse scheduler
-   hotspot adaptive limiting

------------------------------------------------------------------------

# Implementation Requirements

Host must:

-   Parse STATUS frame
-   Maintain chamber and hotspot temps
-   Apply pulse scheduler
-   Apply safety layers
-   Control heater via bitmask

Client must:

-   Read ADS1115
-   Convert NTC temperatures
-   Send STATUS frame periodically

------------------------------------------------------------------------

# End of Specification
