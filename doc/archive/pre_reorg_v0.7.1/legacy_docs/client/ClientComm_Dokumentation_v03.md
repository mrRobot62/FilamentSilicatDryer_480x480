# ClientComm – UART Client Communication Handler (ESP32-WROOM)

**Version:** 0.3  
**Date:** 2026-01-07  
**File:** `ClientComm.cpp` / `ClientComm.h`  
**Role:** CLIENT-side UART protocol handler (ESP32-WROOM) for a Host↔Client link (ESP32-S3 HOST)

---

## Ziel und Kontext

`ClientComm` kapselt die komplette **UART-Link-Kommunikation** auf der CLIENT-Seite:

- RX: Frames vom HOST (Textprotokoll, CRLF-terminated)
- Parsing: Delegiert an `ProtocolCodec::parseLine(...)`
- Reaktion: Set/Update/Tog der Output-Maske, Status-Ausgabe, PING/PONG
- TX: Antworten an den HOST über `Serial2` (oder konfigurierbares `HardwareSerial`)

Wichtig: `ClientComm` ist **non-blocking** und darf permanent in `loop()` laufen.

---

## Architekturüberblick

```mermaid
flowchart LR
    Host[ESP32-S3 HOST] -- "UART RX/TX\nH;SET / H;GET;STATUS / H;PING\nH;UPD / H;TOG" --> ClientComm["ClientComm (ESP32-WROOM)"]
    ClientComm -- "Callbacks\nOutputsChanged / FillStatus\nSerialMonitor / Heartbeat" --> App["Client Sketch / Application"]
    App --> IO[GPIO Outputs CH0..CH15]
    App --> Sensors[ADC + Temp Sensor]
    ClientComm -- "UART TX\nC;ACK;SET / C;ACK;UPD / C;ACK;TOG\nC;STATUS / C;PONG / C;ERR" --> Host
```

---

## Kommunikationsprotokoll (kurz)

Frames sind **ASCII**, durch `\r\n` abgeschlossen.

### Host → Client

| Frame             | Bedeutung                              |
| ----------------- | -------------------------------------- |
| `H;SET;FFFF`      | Setzt komplette 16-bit Maske           |
| `H;UPD;SSSS;CCCC` | Set/Clr Teilmasken (setMask/clearMask) |
| `H;TOG;TTTT`      | Toggles Bits                           |
| `H;GET;STATUS`    | Fordert Status an                      |
| `H;PING`          | Link-Test                              |

### Client → Host

| Frame                            | Bedeutung                         |
| -------------------------------- | --------------------------------- |
| `C;ACK;SET;FFFF`                 | ACK für SET (übernommene Maske)   |
| `C;ACK;UPD;FFFF`                 | ACK für UPD (resultierende Maske) |
| `C;ACK;TOG;FFFF`                 | ACK für TOG (resultierende Maske) |
| `C;STATUS;mask;a0;a1;a2;a3;temp` | Statuspayload                     |
| `C;PONG`                         | Antwort auf PING                  |
| `C;ERR;SET;code`                 | Fehler für SET                    |

---

## Versionshistorie

### V0.3 (2026-01-07)

- **Robuste Verarbeitung eingehender Zeilen**
  - `trim()` auf kompletter Zeile
  - Ignorieren leerer Zeilen
  - Entfernen führender Junk-Bytes bis `H` oder `C`
  - **Wichtig:** Nach `handleIncomingLine(line)` wird **nicht** `return` gemacht, sondern weiter RX gepumpt (`continue`)  
- Stabilität bestätigt durch umfangreiche HOST-TestCases

### V0.2 (2026-01-06)

- Fixes in line processing
- Heartbeat Callback eingeführt

---

## Public API (ClientComm)

### Konstruktor & Setup

- `ClientComm(HardwareSerial &serial, uint8_t rx, uint8_t tx)`
- `begin(uint32_t baudrate)`

### Laufzeit

- `loop()`  
  Non-blocking RX/TX Verarbeitung (muss häufig aufgerufen werden)

### Status-/Event Flags

- `bool hasNewOutputsMask() const`
- `uint16_t getOutputsMask() const`
- `void clearNewOutputsMaskFlag()`

- `bool statusRequested() const`
- `void clearStatusRequestedFlag()`

### Antworten / TX

- `void sendStatus(const ProtocolStatus &status)`
- `void processLine(const String &line)` (Test/Simulation: bypass UART)

### Callbacks (Integration in Anwendung)

- `setOutputsChangedCallback(OutputsChangedCallback cb)`  
  Wird bei SET/UPD/TOG ausgelöst, liefert neue Maske.

- `setFillStatusCallback(FillStatusCallback cb)`  
  Anwendung füllt vor `sendStatus()` ADC/Temp/Mask.

- `setTxLineCallback(TxLineCallback cb)`  
  Optionales SerialMonitor-Logging für gesendete Frames.

- `setHeartBeatCallback(HeartBeatCallback cb)`  
  Optionales “alive”-Signal bei UART Activity.

---

## Ablauf: RX Line Assembly in `loop()`

### Eigenschaften

- Liest alle verfügbaren RX Bytes
- `\r` wird verworfen
- `\n` beendet eine Zeile
- Schutz vor runaway garbage: Buffer-Limit 120 Zeichen

### Wichtige Robustheitsregeln

1. **Zeile trimmen** (`line.trim()`)
2. **Leere Zeilen ignorieren**
3. **Leading Junk entfernen**  
   Entfernt führende Bytes bis ein `H` oder `C` gefunden wird
4. **Keine frühe Rückkehr**  
   Nach Verarbeitung einer Zeile wird weiter gelesen (kein `return`), damit Burst-RX korrekt verarbeitet wird.

```mermaid
flowchart TD
    A[RX bytes available] --> B{read byte}
    B -->|\r| B
    B -->|\n| C[finalize line]
    B -->|other| D[append to rxBuffer]
    D --> E{len>120?}
    E -->|yes| F[drop buffer]
    E -->|no| B
    C --> G[trim + empty check]
    G -->|empty| B
    G --> H[drop leading junk until H/C]
    H -->|no H/C| B
    H --> I["handleIncomingLine(line)"]
    I --> B
```

---

## Ablauf: `handleIncomingLine()`

`handleIncomingLine()` delegiert das Parsing vollständig an `ProtocolCodec::parseLine(...)`.

- Bei Parse-Fehler:
  - Zeile wird geloggt (`RAW("[CLIENT] Failed to parse line: ...")`)
  - Keine ERR-Frame-Rücksendung (bewusst minimal; optional erweiterbar)

### Zustandsdiagramm (Client)

```mermaid
stateDiagram-v2
    [*] --> Idle

    Idle --> OnSet : H;SET
    OnSet --> Idle : C;ACK;SET

    Idle --> OnUpd : H;UPD
    OnUpd --> Idle : C;ACK;UPD

    Idle --> OnTog : H;TOG
    OnTog --> Idle : C;ACK;TOG

    Idle --> OnGetStatus : H;GET;STATUS
    OnGetStatus --> Idle : C;STATUS

    Idle --> OnPing : H;PING
    OnPing --> Idle : C;PONG
```

---

## Sequenzen

### PING/PONG (Link-Test)

```mermaid
sequenceDiagram
    participant Host as HOST
    participant Client as CLIENT (ClientComm)
    Host->>Client: H#semi;PING\r\n
    Client->>Client: parseLine()
    Client-->>Host: C#semi;PONG\r\n
```

### SET + ACK;SET

```mermaid
sequenceDiagram
    participant Host as HOST
    participant Client as CLIENT (ClientComm)
    participant App as Application
    Host->>Client: H#semi;SET#semi;00A5\r\n
    Client->>Client: parseLine()
    Client->>Client: outputsMask=0x00A5, newOutputsMask=true
    Client->>App: OutputsChangedCallback(0x00A5)
    Client-->>Host: C#semi;ACK#semi;SET#semi;00A5\r\n
```

### GET/STATUS + STATUS

```mermaid
sequenceDiagram
    participant Host as HOST
    participant Client as CLIENT (ClientComm)
    participant App as Application
    Host->>Client: H#semi;GET#semi;STATUS\r\n
    Client->>Client: parseLine()
    Client->>App: FillStatusCallback(status)
    App-->>Client: status filled (ADC/temp/mask)
    Client-->>Host: C#semi;STATUS#semi;...\r\n
```

---

## Implementierungsdetails & bekannte Stolpersteine

### 1) „return“ im RX-Pfad vermeiden

In früheren Iterationen war ein `return` nach Zeilenverarbeitung kritisch, weil dann bei Burst-RX nur die erste Zeile verarbeitet wurde.  
V0.3 nutzt konsequent `continue`, um RX vollständig zu konsumieren.

### 2) “Junk”-Bytes / Boot Noise

Auf echten UART-Verbindungen können am Anfang der Kommunikation Steuerzeichen/Junk auftreten.  
V0.3 entfernt führenden Junk bis `H` oder `C`. Das hat sich in den TestCases als stabil erwiesen.

### 3) Heartbeat nur bei Activity

`_heartBeatCb()` wird **nur** aufgerufen, wenn RX Activity vorhanden war (`hadActivity == true`).  
So wird die Applikation nicht geflutet.

---

## Minimaler Integrations-Guide (Client Sketch)

```cpp
#include "ClientComm.h"

static constexpr uint8_t LINK_RX = 16;
static constexpr uint8_t LINK_TX = 17;

ClientComm comm(Serial2, LINK_RX, LINK_TX);

static void onOutputs(uint16_t m) {
  // apply to GPIO
}

static void fillStatus(ProtocolStatus &st) {
  // st.outputsMask = ...
  // st.adcRaw[...] = ...
  // st.tempRaw = ...
}

static void monitor(const String &line, const char *dir) {
  Serial.printf("[CLIENT] [%s]: %s\n", dir, line.c_str());
}

static void hb() {
  // optional: LED pulse / counter
}

void setup() {
  Serial.begin(115200);
  comm.setOutputsChangedCallback(onOutputs);
  comm.setFillStatusCallback(fillStatus);
  comm.setTxLineCallback(monitor);
  comm.setHeartBeatCallback(hb);
  comm.begin(115200);
}

void loop() {
  comm.loop();
}
```

---

## T10.1.40 – Client Safety Watchdog (Host-Timeout)

### Motivation

Der Client (ESP32-WROOM / Powerboard) ist sicherheitskritisch, da er den
**HEATER direkt hardwareseitig steuert** (PWM, 4 kHz).

Ein kritisches Fehlerszenario tritt auf, wenn:
- der **HOST (ESP32-S3)** abstürzt, neu geflasht oder blockiert
- der **Client weiterläuft**
- zuvor **HEATER = ON** gesetzt war

Ohne zusätzliche Schutzmechanismen würde der HEATER **unbegrenzt weiterlaufen**.

→ **Gefahr von Überhitzung und Sachschäden (Severity: HIGH)**

---

### Sicherheitskonzept

Der Client implementiert einen **Host-Watchdog**:

- **Jedes gültige Protokoll-Frame** vom Host gilt als *Lebenszeichen*
  - SET / UPD / TOG
  - GET STATUS
  - PING
- Bleibt dieses Lebenszeichen zu lange aus, wird ein **Hard-Fail-Safe ausgelöst**

---

### Timeout-Definition

```cpp
static constexpr uint32_t CLIENT_HOST_TIMEOUT_MS = 2000; // 2 Sekunden---
## Fazit

`ClientComm` V0.3 ist ein **robuster, non-blocking UART Client-Handler**, der:

- line assembly zuverlässig verarbeitet (inkl. Junk/Boot-Noise)
- Host-Kommandos (SET/UPD/TOG/GET STATUS/PING) korrekt beantwortet
- die Applikation sauber über Callbacks integriert
- und sich im Zusammenspiel mit den HOST-TestCases als stabil bewährt hat.
