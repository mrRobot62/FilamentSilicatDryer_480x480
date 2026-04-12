# BOM Baseline

This page is the active baseline for the project bill of materials.

It is intentionally incomplete in phase 2. The goal here is to establish a maintained structure that separates:

- mandatory core components
- project-specific controller hardware
- still-unverified or optional items

## Core platform

Mandatory baseline items:

- base appliance: EMPHSISM AFTO-1505D or close-equivalent reference unit
- reused original oven power board from that unit
- `HOST` controller platform based on ESP32-S3 with 480x480 touch display
- `CLIENT` controller platform based on ESP32-WROOM

## Sensor and control baseline

Documented project-relevant hardware classes:

- chamber / hotspot related sensor path on the `CLIENT`
- external temperature sensing on the `HOST`
- UART/TTL interconnect between `HOST` and `CLIENT`
- wiring between `CLIENT` and the reused power board

## Current status of the BOM

Already clear at system level:

- appliance family
- two-controller architecture
- reused power board concept
- display family / panel class

Not yet clean enough for a final BOM:

- exact purchasable `HOST` board SKU
- exact purchasable `CLIENT` board SKU
- connector families
- cable assemblies
- mounting hardware
- optional protection components for safer development workflows

## Recommended BOM structure for the next revision

The next maintained BOM version should split parts into:

1. mandatory appliance and reused parts
2. controller boards
3. sensors
4. interconnect and wiring
5. mounting/mechanical parts
6. optional development and safety accessories

## Items that should later be added explicitly

The following items should later appear as real BOM rows:

- exact ESP32-S3 display module
- exact ESP32-WROOM module/board
- sensor part numbers
- USB isolation recommendation for development
- power, signal and sensor harness parts
- screws, spacers and mounting aids

## Images and source material needed

To turn this into a practical BOM, the next revision should be supported by:

- photos of the actually used controller boards
- photos of sensor assemblies
- photos of connectors and harnesses
- optional notes about purchase sources or acceptable alternatives

## Current interpretation rule

Until the BOM is expanded, treat this page as a structural baseline, not as a complete shopping list.
