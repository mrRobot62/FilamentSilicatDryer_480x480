# screen_config – Konfiguration

## Zweck

- Preset auswählen
- Laufzeitparameter (Dauer, Temperatur) anpassen (nicht persistent)

## Workflow


```mermaid
flowchart TD
  Enter["Open screen_config"] --> Init["Populate widgets from OvenRuntimeState"]
  ChangePreset["Select preset"] --> ApplyPreset["oven_select_preset(index)"]
  ChangeTime["Adjust duration"] --> ApplyTime["oven_set_runtime_duration_minutes(min)"]
  ChangeTemp["Adjust target temp"] --> ApplyTemp["oven_set_runtime_temp_target(c)"]

  ApplyPreset --> RS["OvenRuntimeState updated (host plan)"]
  ApplyTime --> RS
  ApplyTemp --> RS

  RS --> Preview["UI shows updated targets (non-persistent)"]
  Note["No direct actuator writes here"] -.-> RS
```


## Wichtige Eigenschaft

- Änderungen betreffen den Host-Plan (Runtime Targets)
- Keine direkten Aktuator-Schreibzugriffe aus diesem Screen
