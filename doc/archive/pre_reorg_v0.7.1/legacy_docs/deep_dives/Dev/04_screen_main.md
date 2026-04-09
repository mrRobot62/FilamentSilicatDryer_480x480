# screen_main – Laufzeitansicht

## Zweck

- Primäre Ansicht im Betrieb
- Zeigt: Zeit, Dial/Countdown, Preset, Icons (Actuators), Temperatur-Skala
- Bedienung: START/STOP sowie WAIT/RESUME

## Workflow


```mermaid
flowchart TD
  Tick["loop() periodic"] --> Poll["oven_comm_poll()"]
  Poll --> Tick1["oven_tick() 1Hz"]
  Tick1 --> UI["screen_main_refresh_from_runtime()"]

  UI --> RS["OvenRuntimeState (snapshot)"]
  RS --> Render["Render: Dial, TimeBar, Icons, TempScale, Buttons"]

  ClickStart["Touch START/STOP"] --> OVENSTART["oven_start() / oven_stop()"]
  ClickWait["Touch WAIT/RESUME"] --> OVENWAIT["oven_pause_wait() / oven_resume_from_wait()"]
  OVENSTART --> COMM["HostComm SET mask"]
  OVENWAIT --> COMM
  COMM --> RS
```


## Regeln

- Icons zeigen Remote Truth (STATUS/ACK), nicht „Wunschzustand“
- Navigation im RUNNING stark eingeschränkt (main ↔ log)
