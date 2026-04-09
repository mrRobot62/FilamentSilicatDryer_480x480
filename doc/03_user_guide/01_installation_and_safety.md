# Installation and Safety

This page is the maintained baseline for installation, commissioning and safe handling of the current hardware setup.

It does not yet replace a future illustrated wiring guide, but it defines the required order, constraints and safety rules for a reproducible bring-up.

## Non-negotiable safety rule

Do not connect USB while the dryer is connected to mains power.

Reason:

- the reused oven power board is mains-related
- the ESP32 controllers are electrically part of that system during operation
- a direct USB connection can create dangerous potential differences
- the result can be damage to the ESP32, the PC, the power board or connected tools

Allowed options for development work:

- disconnect mains completely before USB work
- use a galvanically isolated USB setup
- use an isolated bench supply / isolated system setup

## Scope of the current installation baseline

The current product split is:

- base oven hardware
- reused oven power board
- `HOST`: ESP32-S3 display and touch controller
- `CLIENT`: ESP32-WROOM IO and telemetry controller

The active documentation currently guarantees:

- the system split and responsibilities
- the major safety constraint around USB and mains
- the current firmware/version structure
- the basic commissioning sequence

The active documentation does not yet fully guarantee:

- a final illustrated wiring plan
- a complete BOM with alternatives
- validated hardware variants beyond the documented baseline

## Prerequisites

Before installation, make sure that:

1. the base oven hardware matches the documented reverse-engineered platform closely enough
2. both controller boards are available and flashed with matching firmware generation
3. all work on the open device is done with mains disconnected
4. you have a safe plan for first power-up and fault isolation

Relevant background:

- [Hardware documentation index](../01_reverse_engineering_and_hardware/README.md)
- [Software architecture index](../02_software_architecture/README.md)
- [Release and update workflow](03_release_and_update.md)

## Recommended installation sequence

Use this order for first assembly and first commissioning.

1. Identify and inspect the base hardware.
2. Verify the reused power board against the documented reverse-engineering notes.
3. Mount and wire the `HOST` controller.
4. Mount and wire the `CLIENT` controller.
5. Verify sensor, heater and fan wiring with power disconnected.
6. Flash the intended firmware pair.
7. Perform the first logic-only bring-up without unsafe USB-plus-mains combinations.
8. Validate UI, telemetry and output state behavior before normal operation.

## Bring-up checklist

Before first controlled heating, verify at minimum:

- the display boots and shows the expected firmware generation
- touch input works
- `HOST` and `CLIENT` communication is alive
- door state is reported plausibly
- chamber and hotspot telemetry are plausible
- actuator states shown in the UI match the real hardware state
- the system enters a safe state when a blocking condition is present

## Safe commissioning practice

For first power-up and debug sessions:

- do not start with a fully closed oven and unattended heating
- verify sensor plausibility before enabling thermal runs
- prefer short supervised test runs first
- document anomalies immediately, especially around heater state, fan state and door handling

## Known current documentation gaps

The following items are intentionally tracked as open work and should not be assumed complete yet:

- full point-to-point wiring documentation
- illustrated installation steps
- complete BOM
- validated hardware alternatives
- formal safety analysis beyond the currently documented operational rules

## What will be added next

This page is the phase-1 baseline. The next documentation step should add:

- a dedicated wiring guide
- installation photos
- connector-level mapping
- a reproducible hardware checklist
