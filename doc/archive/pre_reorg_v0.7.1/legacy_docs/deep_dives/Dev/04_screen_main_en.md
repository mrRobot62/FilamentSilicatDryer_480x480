# screen_main – Runtime View

## Purpose

- Primary runtime view during operation
- Shows: time, dial/countdown, preset, actuator icons, temperature scale
- Controls: START/STOP and WAIT/RESUME

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


## Rules

- Icons reflect remote truth (STATUS/ACK), not “desired state”
- Navigation while RUNNING is restricted (main ↔ log)
