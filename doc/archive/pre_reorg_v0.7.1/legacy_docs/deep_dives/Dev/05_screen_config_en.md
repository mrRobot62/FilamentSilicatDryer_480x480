# screen_config – Configuration

## Purpose

- Select preset
- Adjust runtime targets (duration, temperature) (non-persistent)

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


## Key Property

- Changes update the host plan (runtime targets)
- No direct actuator writes from this screen
