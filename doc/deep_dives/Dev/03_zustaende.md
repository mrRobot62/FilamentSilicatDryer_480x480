# Zustände und Flags

## Zustandsautomat (Host-Sicht)


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


## Bedeutungen

- `mode` (STOPPED/RUNNING/WAITING/POST) ist der **primäre** UI-Zustand.
- `running` ist ein Legacy-Flag (Übergangsphase), langfristig durch `mode` ersetzbar.
- `linkSynced` sagt: Handshake stabil (PONG-Streak).
- `commAlive` sagt: RX-Traffic ist frisch genug (Timeout nicht überschritten).

## Safety-Fall

Wenn RX-Traffic ausbleibt:

- Host sendet einmalig „SAFE STOP“ (SET 0x0000)
- UI wird lokal in STOPPED gezwungen (kein „stale green“)
- LinkSync wird zurückgesetzt, damit die Wiederkehr sauber neu synchronisiert
