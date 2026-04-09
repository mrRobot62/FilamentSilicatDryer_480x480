# screen_dbg_hw – Hardware-Testscreen (Safety)

## Zweck

- Testscreen für Hardware/Ports (nur wenn nicht RUNNING)
- Zusätzliche Safety-Mechanismen (RUN-Gate, Swipe-away safe off)
- Optional: komfortableres Togglen über Icon **und** Label

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


## Safety-Details

- `RUN gate` default: **false** (orange)
- Erst wenn `RUN gate` true: Icon/Label toggles senden Requests
- Swipe weg vom Screen: Outputs OFF + RUN gate zurücksetzen
