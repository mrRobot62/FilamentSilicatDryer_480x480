# States and Flags

## Host-Side State Machine


```mermaid
stateDiagram-v2
  [*] --> STOPPED

  STOPPED --> RUNNING: "START"
  RUNNING --> WAITING: "PAUSE / WAIT"
  WAITING --> RUNNING: "RESUME (door closed)"
  WAITING --> STOPPED: "STOP"

  RUNNING --> POST: "Countdown finished AND postPlan.active"
  POST --> STOPPED: "POST finished"
  RUNNING --> STOPPED: "STOP"

  %% Communication safety
  state "COMM ALIVE" as COMMALIVE
  state "COMM LOST" as COMMLOST

  COMMALIVE --> COMMLOST: "no RX traffic > timeout"
  COMMLOST --> COMMALIVE: "handshake stable again"

  COMMLOST: "Host sends SAFE STOP (SET 0x0000)"
  COMMLOST: "UI forced into STOPPED"
```


## Meanings

- `mode` (STOPPED/RUNNING/WAITING/POST) is the primary UI state.
- `running` is a legacy flag (transition period), intended to be replaced by `mode`.
- `linkSynced` means: handshake is stable (PONG streak).
- `commAlive` means: RX traffic is fresh enough (timeout not exceeded).

## Safety Case

When RX traffic stops:

- host sends a one-time “SAFE STOP” (SET 0x0000)
- UI is locally forced into STOPPED (no stale green indicators)
- LinkSync is cleared so recovery re-handshakes cleanly
