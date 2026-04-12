# Reverse Engineering and Hardware

This section documents the physical platform that the software is built around.

Contents:

1. [Power board analysis](01_powerboard_analysis.md)
2. [LVGL display and panel specifics](02_lvgl_display.md)
3. [KiCad hardware files](03_kicad.md)
4. [Hardware setup baseline](04_hardware_setup.md)
5. [Wiring baseline](05_wiring_baseline.md)
6. [BOM baseline](06_bom_baseline.md)

## Current hardware split

- Base appliance: EMPHSISM AFTO-1505D mini oven / air fryer platform
- Reused original power board: heater, fans, lamp, motor, mains-side supply
- New `HOST` board: ESP32-S3 with 480x480 LVGL touch display
- New `CLIENT` board: ESP32-WROOM interface to the reused power board

## Current maturity of this section

This section now covers:

- reverse-engineered context of the reused hardware
- display/panel baseline for the `HOST`
- archived KiCad asset location
- active setup, wiring and BOM baselines for future cleanup

The next hardware documentation step should add:

- annotated wiring photos
- connector-level pin mapping with images
- tested hardware variants and exclusions
- assembly guidance for repeatable rebuilds
