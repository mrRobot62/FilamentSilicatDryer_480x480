# screen_dbg_hw – Hardware Debug Screen (Safety)

## Purpose

- Hardware/port test screen (only when not RUNNING)
- Extra safety mechanisms (RUN gate, swipe-away safe off)
- Optional: easier toggling via icon **and** label

## Workflow


```mermaid
flowchart TD
  Enter["Open screen_dbg_hw"] --> Gate["RUN gate = false (safe)"]
  Gate --> UI0["All outputs forced OFF\nButton shows ORANGE"]
  TapRun["Tap RUN button"] --> GateOn["RUN gate toggles true (ARMED)"]
  GateOn --> UI1["Button shows RED"]

  TapIcon["Tap icon OR label"] --> Req["Request output toggle (test)"]
  Req --> SET["Send mask request via oven.* -> HostComm"]
  SET --> ACK["ACK/STATUS updates remote truth"]
  ACK --> Update["Update icons, rows and wires from runtime"]

  Swipe["Swipe away from DBG_HW"] --> Safe["Safety: force OFF + RUN gate false"]
  Safe --> UI0
```


## Safety Details

- `RUN gate` default: false (orange)
- Only when `RUN gate` true: icon/label toggles send requests
- Swiping away from the screen: outputs OFF + reset RUN gate
