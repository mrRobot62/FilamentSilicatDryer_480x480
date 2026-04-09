# oven_processing_v02_de.md

**Dokument:** oven_processing  
**Version:** v02 (DE)  
**Quelle:** Analyse von `Client.cpp` (ESP32-WROOM Client) + bestehendem Doku-Kontext aus T9/T10  
**Zielgruppe:** Debug/Verständnis – *„Ich sehe ein Log / eine Maske und verstehe sofort, was passiert.“*  
**Stand:** 2026-01-30

---

## Ziel

Dieses Dokument erklärt den **tatsächlichen Verarbeitungsablauf im ESP32‑WROOM Client** (Datei `Client.cpp`):

- Wie `outputsMask` vom Host ankommt (UART/ClientComm)
- Wie daraus **physische Ausgänge** (GPIOs, PWM) werden
- Welche **Safety‑Mechanismen** im Client *autoritatv* greifen
- Wie `STATUS` aufgebaut wird (inkl. Door, ADC, MAX6675)
- Welche Logs bei welchen Zustandswechseln entstehen

> Wichtig: Der Client ist **hardware‑autoritativ**.  
> Der Host darf keinen „gewünschten“ Zustand als Wahrheit anzeigen, sondern rendert nur aus **Client‑Telemetrie**.

---

## 1. Bausteine & Rollen

### 1.1 ClientComm (UART / Protokoll)

`ClientComm` übernimmt:
- UART RX/TX (Serial2: GPIO16/17)
- Parsing der ASCII‑Frames (`SET`, `UPD`, `TOG`, `GET/STATUS`, `PING`)
- Verwaltung des empfangenen `outputsMask`
- Watchdog/Timeout‑Logik (Host silent → Maske wird auf `0x0000` forciert)

Der Anwendungs‑Code (`Client.cpp`) liefert Callbacks:
- `fillStatusCallback(ProtocolStatus&)`
- `outputsChangedCallback(uint16_t newMask)`
- `txLineCallback(const String& line, const String& dir)` (Debug)

---

## 2. Datenfluss: Von UART zur Hardware

### 2.1 High‑Level Ablauf (Loop)

Im `loop()` passiert logisch:

1. `clientComm.loop()` verarbeitet UART (RX/TX, Parser, Timeouts)
2. Wenn sich `outputsMask` geändert hat:
   - Callback `outputsChangedCallback()` markiert nur „pending“
   - **Die Hardware wird NICHT im RX‑Kontext geschaltet**
3. Der Haupt‑Loop übernimmt deterministisch:
   - `applyOutputs(pendingMask)`
4. Zusätzliche Safety‑Schicht:
   - Host‑Timeout → `outputsMask == 0x0000` → **Hard‑Kill**
   - Türwechsel → sofortiges Erzwingen einer sicheren Maske

### 2.2 Mermaid: Pipeline

```mermaid
flowchart TD
  A[UART RX: Host Frames] --> B[ClientComm.loop()]
  B -->|mask changed| C[outputsChangedCallback(newMask)]
  C --> D[g_applyPending=true<br/>g_pendingMask=newMask]
  D --> E[loop(): applyOutputs(pendingMask)]
  E --> F[Door-Gating + PWM + GPIO writes]
  F --> G[g_effectiveMask updated]
  B --> H[fillStatusCallback()]
  G --> H
  H --> I[UART TX: C;STATUS;....]
```

---

## 3. Bitmasken & GPIO-Zuordnung (aus `Client.cpp` / `pins_client.h`)

### 3.1 Logische Bits (CH0..CH7) + DOOR Telemetrie

> `Client.cpp` verarbeitet **8 Bits** (CH0..CH7) als Ausgänge.  
> Das DOOR‑Bit ist **Input‑only Telemetrie** und wird niemals als Output getrieben.

| Bit | Maske | Logischer Name | GPIO | Richtung | Hinweis |
|----:|:-----:|----------------|-----:|:--------:|--------|
| 0 | 0x0001 | FAN12V | 32 | OUT | 12V‑Board‑Kühlung |
| 1 | 0x0002 | FAN230V | 33 | OUT | 230V Fan „fast“ |
| 2 | 0x0004 | FAN230V_SLOW | 27 | OUT | 230V Fan „slow“ |
| 3 | 0x0008 | SILICAT_MOTOR | 26 | OUT | Motor |
| 4 | 0x0010 | HEATER | 12 | OUT (PWM) | PWM‑Heater (LEDC) |
| 5 | 0x0020 | LAMP | 25 | OUT | Lampe |
| 6 | 0x0040 | DOOR_ACTIVE | 14 | IN | Tür offen/aktiv (active‑high) |

---

## 4. Kernfunktion: `applyOutputs(requestedMask)`

### 4.1 Schritte in `applyOutputs()`

1. Türzustand lesen: `doorOpen = isDoorOpen()`
2. **Safety‑Gating** anwenden: `eff = applyDoorSafetyGating(requestedMask, doorOpen)`
3. Physische Ausgänge setzen:
   - Alle Bits außer DOOR (Input‑only)
   - Special Case HEATER: **nur PWM**, kein `digitalWrite()` auf dem Heater‑Pin
4. Shadow/Telemetrie‑Maske setzen: `g_effectiveMask = eff`
5. PWM‑Wahrheit einpflegen:
   - Wenn PWM läuft, wird HEATER‑Bit in `g_effectiveMask` gesetzt
   - Wenn PWM aus ist, wird es gelöscht

### 4.2 Door‑Safety‑Gating (`applyDoorSafetyGating()`)

Regeln in `Client.cpp`:

- DOOR‑Bit wird **immer** aus der Output‑Maske entfernt (Input‑only)
- Wenn Tür offen:
  - HEATER OFF
  - SILICAT_MOTOR OFF
  - FAN230V OFF (*explizite Regel im Code*)
  - FAN230V_SLOW wird **nicht** erzwungen (bleibt wie angefordert)

---

## 5. Türsignal: `isDoorOpen()`

- Tür wird über `OVEN_DOOR_SENSOR` gelesen (GPIO14)
- Definition: `DOOR_OPEN_IS_HIGH`  
  → im aktuellen Stand: **OPEN = HIGH**, CLOSED = LOW
- Zustandsänderungen werden geloggt:

Beispiel:
- `[DOOR] state=OPEN (level=1)`
- `[DOOR] state=CLOSED (level=0)`

### 5.1 Türwechsel erzwingt sichere Ausgänge (zusätzlich)

Im `loop()` wird bei Tür‑Transition nach OPEN zusätzlich geprüft:

- Falls die aktuelle Maske unsicher wäre, wird sofort:
  - die sichere Maske berechnet
  - `applyOutputs(after)` ausgeführt
- Falls PWM oder Motor physisch noch aktiv sind, wird „Hard‑Kill“ gemacht:
  - `heaterPwmEnable(false)`
  - Motor‑Pin LOW
  - Shadow‑Maske entsprechend bereinigt

---

## 6. Heater-PWM (LEDC) – `heaterPwmEnable(enable)`

Der Heater wird **nicht** als normales GPIO‑On/Off geschaltet, sondern via **LEDC PWM**.

Ziele (aus Codekommentaren):
- deterministischer PWM‑Start nach Boot
- „clean attach“ (Detach → Attach → Duty setzen)
- definierter Safe‑Level am Pin beim Start/Stop

> Konsequenz für Logs & Masken:  
> `g_effectiveMask` wird nach dem Apply **mit PWM‑Wahrheit** korrigiert.

---

## 7. STATUS-Erzeugung (Telemetrie)

`fillStatusCallback(ProtocolStatus &st)` erzeugt die Statusdaten:

- `st.outputsMask = g_effectiveMask` (nicht direkt aus ClientComm)
- DOOR‑Bit wird anhand des Inputs gesetzt/gelöscht
- `adcRaw[0] = analogRead(PIN_ADC0)` (typisch 0..4095)
- `tempRaw = readTempRaw_QuarterC()` (optional MAX6675)
  - Einheit: **0.25°C Schritte** (`tempRaw = °C * 4`)

### 7.1 Over-Temperature Safety

Nach dem Lesen der Temperatur:
- `tempCur = tempRaw * 0.25`
- `safety_check_overtemp(tempCur)`:
  - wenn `tempCur >= CLIENT_ABS_MAX_TEMP_C`
  - dann **Hard OFF**: `heaterPwmEnable(false)`
  - und Fehlerlog: `[SAFETY] ABS OVER-TEMP! ... -> HEATER OFF`

Diese Logik ist **autoritatv im Client**.

---

## 8. Host-Watchdog HARD-KILL (T10)

Im `loop()` gibt es eine zusätzliche harte Safety‑Schicht:

Wenn `clientComm.hasNewOutputsMask()` und `clientComm.getOutputsMask() == 0x0000`:

- Heater PWM wird **physisch** abgeschaltet (falls noch läuft)
- Motor wird explizit auf LOW gezogen
- `g_effectiveMask` wird auf `0x0000` gesetzt

Ziel:
- Wenn Host „weg“ ist oder Timeout greift, ist der Client garantiert in einem **absolut sicheren Zustand**.

---

## 9. Debug-Logs: Masken lesbar machen

`Client.cpp` ergänzt Debug‑Logs um ein 8‑Bit‑Stringformat (CH7..CH0):

- `bitmask8_to_str(mask)` → `"01011001"` (Beispiel)
- Callback `outputsChangedCallback()` loggt pending mask
- `txLineCallback()` versucht, bei bekannten Frames die Maske zu extrahieren und für Debug zu annotieren

Praktische Interpretation:
- Die String‑Maske ist **CH7..CH0**, nicht CH0..CH7
- Für die Zuordnung nutze die Bit‑Tabelle oben

---

## 10. Anhang: Relevante Konstanten (Kurzreferenz)

### 10.1 UART

- `CLIENT_RX2 = GPIO16`
- `CLIENT_TX2 = GPIO17`

### 10.2 Sensorik & ADC

- `PIN_ADC0 = GPIO36` (ADC Raw in `adcRaw[0]`)
- MAX6675 optional:
  - SCK GPIO18
  - CS GPIO5
  - SO GPIO19

---

**EOF**
