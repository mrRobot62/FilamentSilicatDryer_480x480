# ESP32-S3 Filament & SilicaGel Dryer UI

> Eine deutsche Fassung ist in [README.md](README.md) verfuegbar.
> This file is the English companion README.

## Current Release

- Version: `0.7.3`
- Focus:
  - configurable host-side parameters screen
  - NVM-persistent fast filament shortcuts
  - configurable heater curve presets on the host
  - display timeout with dimming and wake-on-touch protection
  - host-side long-term CSV aggregation via `CSV_LR_HOST`
  - expanded active hardware, wiring and BOM baseline documentation

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
- `CSV long-term` with:
  - `Longrun CSV` `OFF/ON`
  - `Intervall (s)` for `10`, `30`, `60`, `300`

Behavior:

- `SAVE` writes the current values to HOST NVM and reboots
- `RESET` restores factory defaults, writes them to NVM and reboots
- the first touch after a dimmed display only wakes the UI and is intentionally consumed

## Documentation

- documentation index: [doc/README.md](doc/README.md)
- software architecture: [doc/02_software_architecture/README.md](doc/02_software_architecture/README.md)
- user guide: [doc/03_user_guide/README.md](doc/03_user_guide/README.md)
- installation and safety: [doc/03_user_guide/01_installation_and_safety.md](doc/03_user_guide/01_installation_and_safety.md)
- release and update workflow: [doc/03_user_guide/03_release_and_update.md](doc/03_user_guide/03_release_and_update.md)

## Safety

Do not connect USB while the device is connected to mains power.

If USB debugging is required:

- disconnect mains completely, or
- use galvanic USB isolation, or
- use an isolated power setup for the device
