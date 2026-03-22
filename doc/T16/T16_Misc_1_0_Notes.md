# T16.Misc.1.0

## Ziel
Produktive, allgemeingültige Schnittstellen für Sensor-NTC und Heater-I/O vorbereiten, ohne bereits die Host-Regelung oder die laufende Client-Logik umzubauen.

## Geänderte Dateien
- `include/sensors/sensor_ntc.h`
- `include/heater_io.h` **(neu)**
- `include/T15/heater_io.h`
- `src/client/t15/sensor_ntc.cpp`
- `src/client/t15/heater_io.cpp`

## Inhalt der Änderung
### 1) `sensor_ntc`
- neutrales Namespace-Zielbild eingeführt: `sensor_ntc`
- neutralen Datentyp `sensor_ntc::Sample` eingeführt
- neue neutrale API eingeführt:
  - `sensor_ntc::sample_temperatures()`
- Kompatibilität für T15 bleibt erhalten durch:
  - `using ChamberSample = Sample;`
  - `namespace t15_sensor = sensor_ntc;`
  - `inline sample_chamber_temperature()` als Wrapper

### 2) `heater_io`
- neue allgemeine Header-Datei `include/heater_io.h`
- neutrales Namespace-Zielbild eingeführt: `heater_io`
- Default-Duty zentral als `kDefaultDutyPercent = 50`
- Kompatibilität für T15 bleibt erhalten durch:
  - `namespace t15_heater = heater_io;`
  - `include/T15/heater_io.h` ist jetzt nur noch ein Wrapper auf `heater_io.h`

## Erwartete Wirkung
- Host-Build: keine fachliche Änderung
- Client-Build: keine fachliche Änderung
- T15-Testcode bleibt nutzbar
- Grundlage für nächsten Schritt geschaffen:
  - Client kann später von interner Direktlogik auf `sensor_ntc` und `heater_io` umgestellt werden

## Sinnvolle Tests
### Build-Test
1. Host bauen
2. Client bauen

**Erwartung:**
- beide Builds bleiben compile-fähig
- keine neue Protokolländerung
- kein geändertes Laufzeitverhalten

### T15-Kompatibilitätstest
1. Optional T15-Testumgebung bauen
2. Prüfen, ob alte Namen weiterhin funktionieren:
   - `t15_sensor::get_sample()`
   - `t15_sensor::sample_chamber_temperature()`
   - `t15_heater::pwm_start()`
   - `t15_heater::pwm_stop()`

**Erwartung:**
- vorhandener T15-Code kompiliert weiter ohne Quelländerung

## Nächster geplanter Schritt
`T16.Client.1.0`
- FSD_Client.cpp schrittweise auf produktive Sensor-API umstellen
- noch ohne Host-Reglerumbau
