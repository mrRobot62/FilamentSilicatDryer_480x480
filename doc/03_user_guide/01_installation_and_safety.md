# Installation and Safety

## Critical safety rule

Do not connect USB while the dryer is connected to mains power.

Reason:

- the reused oven power board is mains-related
- the ESP32 electronics can see unsafe potential differences during development
- this can damage the ESP32, the PC or both

Safe options:

- unplug mains before USB work
- use a USB isolator
- use an isolated supply setup

## System overview for installation

The current setup consists of:

- base oven hardware
- reused power board
- `HOST` display controller
- `CLIENT` IO controller

## Installation baseline

The archived installation notes are still available here:

- `doc/archive/pre_reorg_v0.7.1/legacy_docs/filament-silicagel-dryer_installation_v2.md`

That content should later be rewritten into a compact, maintained installation guide once the hardware variant is frozen.
