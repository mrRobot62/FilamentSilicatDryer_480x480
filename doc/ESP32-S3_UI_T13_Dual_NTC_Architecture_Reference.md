# T13 -- Dual‑NTC Architecture Reference (Clean Context, No UDP Logging)

## Overview

T13 introduces a robust and physically correct temperature control
architecture using two separate NTC sensors with clearly defined roles:

-   Chamber Sensor → Temperature Control
-   Hotspot Sensor → Safety Monitoring

This architecture ensures safe filament drying without overshoot and
protects hardware from overheating.

------------------------------------------------------------------------

## System Architecture

ESP32‑S3 Host (UI + Control)\
↕ UART (ProtocolCodec, ASCII CRLF)\
ESP32‑WROOM Client (Sensor Acquisition + Power Control)

### Responsibilities

**Host (ESP32‑S3)**

-   Runs heater control algorithm
-   Maintains OvenRuntimeState (Single Source of Truth)
-   Updates UI
-   Sends actuator commands

**Client (ESP32‑WROOM)**

-   Reads sensors via ADS1115
-   Controls physical outputs
-   Sends STATUS telemetry frames

------------------------------------------------------------------------

## Dual‑NTC Sensor Concept

### Chamber NTC (Control Sensor)

Location: Chamber air\
Purpose: Primary control variable

Characteristics:

-   Represents real filament temperature
-   Stable and slow response
-   Used exclusively for heater control

------------------------------------------------------------------------

### Hotspot NTC (Safety Sensor)

Location: Near heater\
Purpose: Safety monitoring only

Characteristics:

-   Detects local overheating
-   Not used for temperature regulation
-   Protects hardware and filament

------------------------------------------------------------------------

## Explicit Scope of T13

Included:

-   Dual‑NTC measurement architecture
-   Stable temperature regulation
-   Safety‑aware heater control
-   Extended telemetry protocol

Explicitly excluded:

-   UDP logging
-   WiFi features
-   Debug infrastructure

------------------------------------------------------------------------

## Stability Status

Validated:

-   Dual‑NTC measurement working
-   Stable heater control
-   Correct telemetry flow
-   Safe regulation behavior

Ready as foundation for T14 SafetyGuard implementation.
