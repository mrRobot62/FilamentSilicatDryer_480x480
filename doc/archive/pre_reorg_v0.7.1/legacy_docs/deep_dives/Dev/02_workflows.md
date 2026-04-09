# Workflow: UI ↔ oven ↔ HostComm

## Überblick

Dieses Diagramm zeigt den „roten Faden“ der Architektur: UI ist Darstellung, `oven.*` ist Wahrheit, `HostComm` ist Transport.


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


## Praktische Regeln

- UI liest `OvenRuntimeState` und rendert.
- User-Aktionen rufen **nur** API-Funktionen in `oven.*` auf (Policy/Requests).
- `oven_comm_poll()` ist der einzige RX-Pfad (UART, non-blocking).
- `oven_tick()` ist der einzige Ort, an dem der Countdown läuft (1 Hz).

## ACK vs STATUS

- `STATUS` liefert periodisch Telemetrie.
- `ACK` kann für „schnellere UI-Rückmeldung“ genutzt werden, bleibt aber weiterhin Remote Truth.
