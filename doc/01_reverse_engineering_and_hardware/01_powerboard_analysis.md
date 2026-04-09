# Power Board Analysis

## Goal

The original oven power board is reused instead of being redesigned from scratch. The reverse-engineering work exists to understand:

- power conversion and low-voltage supply
- actuator paths for heater, fans, lamp and motor
- door and sensor signals
- risks when connecting custom ESP32 hardware

## What is currently known

The reverse-engineering documents archived from the previous structure show a consistent pattern:

- the board contains the mains-side power stage and the low-voltage supply
- the heater is switched through a relay path
- 230 V loads such as lamp, motor and fan stages are driven through optocoupler / triac style paths
- the board exposes low-voltage control and status connections that are now driven by the `CLIENT`

## Functional block view

```mermaid
flowchart TD
    AC["230 V AC input"] --> PSU["Offline PSU section"]
    PSU --> LV["Low-voltage rail"]
    LV --> CLIENT["ESP32-WROOM client board"]

    CLIENT --> RELAY["Heater relay driver"]
    CLIENT --> TRIAC["Triac / optocoupler outputs"]
    CLIENT --> INPUTS["Door and sensor inputs"]

    RELAY --> HEATER["Heater"]
    TRIAC --> FAN12["12 V fan"]
    TRIAC --> FAN230["230 V fan"]
    TRIAC --> MOTOR["Silica basket motor"]
    TRIAC --> LAMP["Lamp"]

    INPUTS --> CLIENT
```

## Control lines used by the current firmware

The active output bit mapping is defined in `include/output_bitmask.h`:

- `BIT_FAN12V`
- `BIT_FAN230V`
- `BIT_LAMP`
- `BIT_SILICA_MOTOR`
- `BIT_FAN230V_SLOW`
- `BIT_DOOR`
- `BIT_HEATER`

The current `CLIENT` firmware treats the power board as hardware-authoritative:

- commands arrive from `HOST`
- output application happens on `CLIENT`
- telemetry and effective output truth are reported back to `HOST`
- safety gating remains on the `CLIENT`

## Important safety note

The board is mains-related. Existing project warnings remain valid:

- do not attach USB while the oven is connected to mains
- use isolation or disconnected mains supply during development
- treat the board as non-trivial mains hardware, not as a benign low-voltage module

## Source of historic details

Detailed pin traces, annotated photos and earlier analysis notes remain in:

- `doc/archive/pre_reorg_v0.7.1/legacy_docs/reverse_engineering/`
- `doc/archive/pre_reorg_v0.7.1/legacy_docs/images/`
