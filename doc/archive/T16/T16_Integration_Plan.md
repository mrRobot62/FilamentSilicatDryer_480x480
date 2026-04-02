# T16 – Integration HeaterCurve in echte Host/Client Firmware

## Ziel
Saubere Übernahme der T15-Erkenntnisse in die produktive Firmware mit klarer Trennung:
- Host: Heizlogik (Single Source of Truth)
- Client: Sensorik, Aktoren, Status

---

## Architektur-Grundprinzipien

### Host (ESP32-S3)
- Heizlogik / Regler
- Runtime-State
- Entscheidung: Heater ON / OFF
- Relaisfreundliche Steuerung (Min ON/OFF Zeiten)

### Client (ESP32-WROOM)
- Sensoren erfassen (NTC)
- Aktoren sicher ansteuern
- Status melden
- Umsetzung Heater ON → 4kHz Signal (~50% Duty, nie 100%)

---

## Sensor-Konzept

### Chamber (Control)
- Führungsgröße
- Anzeige im UI
- Regelgröße

### Hotspot (Safety)
- Übertemperatur-Erkennung
- Safety-Abschaltung

---

## Wichtige Hardware-Regel
- Kein Leistungs-PWM!
- Heater ist binär ON/OFF
- 4kHz Signal = Steuersignal (ca. 50% Duty)
- Keine schnellen Schaltzyklen
- Mindest-EIN/AUS-Zeiten zwingend

---

## Struktur-Empfehlung

### Allgemein
- include/sensors/sensor_ntc.*
- include/heater_io.h
- src/client/heater_io.cpp

### Client
- src/client/FSD_Client.cpp

### Host
- src/app/oven/oven.cpp

---

## T16 Phasenplan

### Phase A – Basis
**T16.Misc.1.0**
- Sensor-API bereinigen
- Heater-I/O aus T15 extrahieren
- Keine Funktionsänderung

---

### Phase B – Client
**T16.Client.1.0**
- Sensorintegration

**T16.Client.1.1**
- Heater-I/O Integration

**T16.Client.1.2**
- Logging / Status

---

### Phase C – Host
**T16.Host.1.0**
- Heizlogik Umbau

**T16.Host.1.1**
- Mindestzeiten

**T16.Host.1.2**
- Safety & UI

---

## Wichtige Designregeln
- Kein Duty-Control im Host
- Keine Heizlogik im Client
- Trennung:
  - Requested (Host)
  - Actual (Client)

---

## Risiken vermeiden
- Kein Copy-Paste von T15-Testcode
- Keine PWM-Interpretation des Heaters
- Keine Vermischung von Control und Safety

---

## Fazit
T16 ist eine strukturierte Rückintegration von T15 in eine produktionsnahe Architektur mit klarer Verantwortungsaufteilung.
