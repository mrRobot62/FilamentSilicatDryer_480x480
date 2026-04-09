# Documentation

This folder was restructured on top of the `v0.7.1` code line.

The active documentation is intentionally reduced to three maintained sections:

1. [Reverse Engineering and Hardware](01_reverse_engineering_and_hardware/README.md)
2. [Software Architecture](02_software_architecture/README.md)
3. [User Guide](03_user_guide/README.md)

Legacy material is still available in the archive:

- [Archive snapshot before the reorganization](archive/pre_reorg_v0.7.1/README.md)

## Scope of the active documentation

The new structure reflects the current product split:

- `HOST`: ESP32-S3 UI controller with LVGL, runtime state, parameter storage and communication control
- `CLIENT`: ESP32-WROOM hardware controller for outputs, telemetry and safety gating
- Reverse-engineered oven power board reused as actuator and supply stage

## Recommended next additions

The requested structure is complete for the current start. The next useful additions would be:

- a dedicated manufacturing and wiring guide
- a test and validation section
- a release and update workflow section
