# T16.Host.1.0 – Heater Request/Actual sauber trennen

## Ziel
Dieser Schritt zieht im Host die **strukturelle T16-Trennung** ein:

- `heater_request_on` = Host-Entscheidung / Command-Intent
- `heater_actual_on`  = Client-Telemetrie / tatsächlicher Zustand
- `heater_on` bleibt **nur noch als Legacy-UI-Alias** erhalten:
  - während `RUNNING` -> zeigt Request
  - sonst -> zeigt Actual

Zusätzlich werden die Temperaturen expliziter benannt:
- `tempChamberC` = Chamber / Control
- `tempHotspotC` = Hotspot / Safety

Die alten Felder bleiben als Alias erhalten:
- `tempCurrent` -> Alias für `tempChamberC`
- `tempNtcC`    -> Alias für `tempHotspotC`

## Enthaltene Dateien
- `include/oven.h`
- `src/app/oven/oven.cpp`

## Was angepasst wurde

### 1) `include/oven.h`
`OvenRuntimeState` erweitert um:
- `float tempChamberC`
- `float tempHotspotC`
- `bool tempChamberValid`
- `bool tempHotspotValid`
- `bool heater_request_on`
- `bool heater_actual_on`

Bestehende Felder bleiben absichtlich erhalten:
- `tempCurrent`
- `tempNtcC`
- `heater_on`

Damit bleibt bestehende UI kompatibel.

### 2) `src/app/oven/oven.cpp`
Neue kleine Helper:
- `runtime_sync_legacy_temperature_aliases()`
- `runtime_sync_heater_alias()`

Damit wird zentral sichergestellt:
- Alias-Felder bleiben konsistent
- neue T16-Felder sind die eigentliche Quelle

### 3) Telemetrie-Mapping sauberer gemacht
In `apply_remote_status_to_runtime(...)`:
- Chamber/Hotspot werden explizit in:
  - `tempChamberC`
  - `tempHotspotC`
  geschrieben
- Valid-Flags werden aus `TEMP_INVALID_DC` abgeleitet
- `heater_actual_on` kommt aus Client-STATUS
- `heater_request_on` kommt aus Host-Intent
- `heater_on` wird danach nur noch über den Alias-Helper gesetzt

### 4) Host-Heaterentscheidung strukturell getrennt
Die bestehende Heizentscheidung bleibt funktional weitgehend gleich,
aber im Runtime-State wird nun sauber unterschieden zwischen:
- Request
- Actual

Das ist die Grundlage für T16.Host.1.1 und spätere Regler-/Mindestzeit-Schritte.

## Erwartung nach dem Schritt
- HOST Build: ok
- CLIENT Build: ok
- bestehende UI läuft weiter
- kein Protokollumbau nötig
- kein Client-Umbau nötig
- `heater_on` verhält sich aus UI-Sicht wie bisher oder klarer

## Sinnvolle Tests

### 1. Compile-Test
- HOST bauen
- CLIENT bauen

Erwartung:
- beide Builds erfolgreich

### 2. Idle / STOPPED
Erwartung:
- `heater_request_on = false`
- `heater_actual_on = false`
- `heater_on = false`

### 3. RUNNING ohne Safety-Problem
Erwartung:
- Host setzt `heater_request_on` gemäß bestehender Hysterese
- `heater_on` folgt im RUNNING-Modus dem Request
- `heater_actual_on` folgt mit Telemetrie/ACK vom Client

### 4. Door offen während RUNNING
Erwartung:
- `heater_request_on` wird false
- `heater_actual_on` fällt per STATUS/ACK ab
- `heater_on` wird false

### 5. Temperatur-Logging
Erwartung:
- Chamber = Regel-/UI-Größe
- Hotspot = Safety-Größe
- keine Regression auf `-32768`, wenn Sensorpfade ok sind

## Kurze Zusammenfassung
In T16.Host.1.0 wird die Host-Seite auf die T16-Zielstruktur vorbereitet:
explizite Trennung von Heater-Request und Heater-Actual, plus klare Chamber/Hotspot-Benennung,
ohne die bestehende UI oder das Protokoll zu brechen.

## Commit
`T16.Host.1.0 separate heater request/actual in host runtime state`
