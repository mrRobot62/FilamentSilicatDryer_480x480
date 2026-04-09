# ESP32-S3 Filament & SilicaGel Dryer UI

> Eine deutsche Fassung ist in [README.md](README.md) verfuegbar.
> This file is the English companion README.

## Current Release

- Version: `0.7.1`
- Focus:
  - configurable host-side parameters screen
  - NVM-persistent fast filament shortcuts
  - configurable heater curve presets on the host
  - display timeout with dimming and wake-on-touch protection

## Language

- German version: [README.md](README.md)
- English version: [README_EN.md](README_EN.md)

## Project Summary

This project replaces the original oven UI and control surface of the EMPHSISM AFTO-1505D mini oven / air fryer with a two-controller architecture:

- HOST/UI: ESP32-S3 with 480x480 touch display and LVGL
- CLIENT/IO: ESP32-WROOM handling relays, sensors and actuator logic
- communication between both boards via UART/TTL

The system is designed for:

- drying filament spools
- regenerating silica gel
- keeping the original power electronics while replacing the original user interface

## Parameters Screen

The unused LOG screen was replaced by the new `Parameter` screen.

Currently configurable:

- fast shortcut assignment for the four filament buttons shown in `screen_main`
- heater curve parameters for presets `45C`, `60C`, `80C`, `100C`
- `Display timeout` with:
  - `Dimmfaktor (%)`
  - `Dimm-Timeout (min)`

Behavior:

- `SAVE` writes the current values to HOST NVM and reboots
- `RESET` restores factory defaults, writes them to NVM and reboots
- the first touch after a dimmed display only wakes the UI and is intentionally consumed

## Documentation

- German parameters screen doc: [doc/screens/screen_parameters.de.md](doc/screens/screen_parameters.de.md)
- Main screen DE: [doc/screens/screen_main/screen_main.de.md](doc/screens/screen_main/screen_main.de.md)
- Main screen EN: [doc/screens/screen_main/screen_main.en.md](doc/screens/screen_main/screen_main.en.md)

## Safety

Do not connect USB while the device is connected to mains power.

If USB debugging is required:

- disconnect mains completely, or
- use galvanic USB isolation, or
- use an isolated power setup for the device
