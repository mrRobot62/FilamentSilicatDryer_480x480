# Software Architecture

This section describes the current system architecture on top of release `v0.7.3`.

Contents:

1. [HOST architecture](01_host_architecture.md)
2. [CLIENT architecture](02_client_architecture.md)
3. [Communication protocol](03_communication_protocol.md)
4. [Long-term CSV sampling](04_long_term_csv_sampling.md)

## Architectural baseline

The code base is intentionally split into two firmware targets:

- `env:host_esp32s3_st7701`
- `env:client_esp32_wroom`

Shared protocol and communication code lives in `src/share/` and `include/`.
