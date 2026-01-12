# ESP32-S3 UI – T6 Zusammenfassung

## Überblick

Dieser Stand dokumentiert den **abgeschlossenen Entwicklungsstand T6** des Projekts **ESP32-S3 Filament Dryer UI**.
T6 definiert die **Architektur, Kommunikationslogik und Zustandsführung**, stellt jedoch **keinen finalen Produktstand** dar.
Alle Entscheidungen dienen als **stabile Referenzbasis** für die Weiterentwicklung in **T7**.

---

## Systemarchitektur

### Rollenverteilung

- **ESP32-S3 (Host)**
  - UI (LVGL 9.x)
  - Zustandsverwaltung (`oven.cpp`)
  - Countdown-Logik
  - UART-Kommunikation über `HostComm`
  - Zeigt ausschließlich *Ist-Zustand*

- **ESP32-WROOM (Client / Powerboard)**
  - Aktuatorsteuerung
  - Sensorsampling (Temperatur, Tür, ADC)
  - Autoritative Quelle für Aktuator-Zustände

---

## Kommunikationslink

### Transport

- UART (Serial2) mit expliziten RX/TX Pins
- ASCII-basiertes Protokoll
- CRLF-terminierte Textzeilen
- Non-blocking Verarbeitung

### Validierungsstatus

Der Host–Client-Link wurde **vollständig über TestCases validiert**:

- PING / PONG & Link-Synchronisation
- SET / UPD / TOG + ACK
- STATUS / GET
- RST
- Junk & Fragmentierung
- Stress- & Timing-Tests
- Visuelle IO-Tests
- ADC / Temperatur-Plausibilität

➡️ **Der UART-Link gilt als stabil und produktionsreif.**

---

## Protokollverarbeitung

### ProtocolCodec

- Einzige Stelle mit Protokollwissen
- Parsing & Serialisierung aller Frames
- Keine Protokolllogik in UI, oven oder HostComm

### HostComm

- UART RX line-based (`\n`)
- Non-blocking Polling
- Shadow-State (`ProtocolStatus`)
- Sendet:
  - SET
  - GET;STATUS
  - PING
- Meldet:
  - neue STATUS-Daten
  - ACK
  - Comm-Fehler

---

## Zustandsmodell

### Single Source of Truth

```text
Client STATUS → oven.cpp → OvenRuntimeState → UI
```

- `runtimeState` wird **nur** aus `C;STATUS` aktualisiert
- Kein lokales „Vorgaukeln“ von Aktuatorzuständen
- UI rendert ausschließlich `OvenRuntimeState`

### Host-seitige Logik

Bleibt lokal im Host:

- Countdown (`secondsRemaining`, `durationMinutes`)
- Preset-Auswahl
- WAIT / RUN / STOP Zustände
- UI-Interaktion

---

## Bitmasken / Aktuatoren

### Typsichere Definition

```cpp
enum class OVEN_CONNECTOR : uint16_t {
    FAN12V        = 1u << 0,
    FAN230V       = 1u << 1,
    FAN230V_SLOW  = 1u << 2,
    SILICAT_MOTOR = 1u << 3,
    HEATER        = 1u << 4,
    LAMP          = 1u << 5,
    DOOR_ACTIVE   = 1u << 6
};
```

- Keine GPIO-Kenntnis im Host
- Reine Bitmasken-Logik

---

## Aktuatorsteuerung

### Grundprinzip

- Host sendet **SET-Masken**
- Client entscheidet & meldet Realität per STATUS
- Host zeigt ausschließlich Ist-Zustand

### Keine Soll/Ist-Doppelverwaltung

- Kein `desiredOutputsMask`
- Nur Cache:
  - `g_remoteOutputsMask` (letzter STATUS)
  - `g_lastCommandMask` (letzte gesendete Maske)

---

## WAIT / RESUME

### WAIT

- Host friert Countdown ein
- Sendet **WAIT-Policy-Maske**:
  - Heater OFF
  - Motor OFF
  - FAN12V ON
  - FAN230_SLOW ON
  - LAMP ON
  - FAN230 OFF

### RESUME

- Blockiert bei `DOOR_ACTIVE`
- Stellt letzte Command-Maske wieder her
- Countdown läuft weiter

➡️ Keine lokalen Aktuatoränderungen im WAIT/RESUME

---

## Kommunikationsdiagnose

Erweiterungen im `OvenRuntimeState`:

- `commAlive`
- `lastStatusAgeMs`
- `statusRxCount`
- `commErrorCount`

- Alive-Timeout ca. 1500 ms
- Rein beobachtend (keine harte Abschaltung)

---

## main.cpp Integration

### setup()

```cpp
oven_init();
oven_comm_init(Serial2, baudrate, RX_PIN, TX_PIN);
ui_init();
```

### loop()

```cpp
lv_tick_inc(...);
oven_comm_poll();
oven_tick();
screen_main_update_runtime();
lv_timer_handler();
```

---

## Abgrenzung T6 → T7

### T6

- Architektur & Grundlagen
- Kommunikationsmodell
- Zustandsphilosophie
- Referenzstand

### T7 (Folgeschritt)

- Schließen offener Punkte
- Vereinfachung & Stabilisierung
- UI-Verhalten bei Link-Loss
- Vorbereitung Persistenz / NVS

---

## Fazit

T6 definiert ein **robustes, deterministisches und gut testbares Fundament**:

- klare Zuständigkeiten
- kein impliziter Zustand
- kein verstecktes Protokollwissen
- Client autoritativ, Host beobachtend

Dieser Stand dient als **fixe Referenzbasis** für alle weiteren Entwicklungsschritte.
