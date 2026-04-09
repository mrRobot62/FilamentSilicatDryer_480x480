# Workflow: UI ↔ oven ↔ HostComm

## Overview

This diagram shows the core architecture: UI is presentation, `oven.*` holds the truth, `HostComm` is transport.


```mermaid
flowchart TD
  U["User Input (Touch)"] --> SM["ScreenManager (Routing, Swipe)"]
  SM --> SMAIN["screen_main (Read-only runtime view)"]
  SM --> SCFG["screen_config (Edit runtime targets)"]
  SM --> SDBG["screen_dbg_hw (Test UI, safety gated)"]
  SM --> SLOG["screen_log (Diagnostics)"]

  subgraph Core["Core Architecture"]
    OVEN["oven.* (Single Source of Truth)"]
    COMM["HostComm (UART protocol)"]
    CODEC["ProtocolCodec (Parse/Format)"]
  end

  %% Data flow: UI reads, never owns truth
  SMAIN -->|"reads"| OVEN
  SCFG -->|"reads / writes runtime targets"| OVEN
  SDBG -->|"requests outputs (test mode)"| OVEN
  SLOG -->|"reads diag counters"| OVEN

  %% Comms loop
  OVEN -->|"requests: SET/GET/STATUS/PING"| COMM
  COMM --> CODEC
  CODEC --> COMM
  COMM -->|"telemetry: STATUS, ACK, PONG"| OVEN

  %% Key rule
  OVEN -. "Actuator booleans updated only from STATUS/ACK" .-> SMAIN
```


## Practical Rules

- UI reads `OvenRuntimeState` and renders.
- User actions call **only** `oven.*` APIs (policy/requests).
- `oven_comm_poll()` is the single RX path (UART, non-blocking).
- `oven_tick()` is the only place where the countdown runs (1 Hz).

## ACK vs STATUS

- `STATUS` delivers periodic telemetry.
- `ACK` can be used for faster UI feedback, but it still reflects remote truth.
