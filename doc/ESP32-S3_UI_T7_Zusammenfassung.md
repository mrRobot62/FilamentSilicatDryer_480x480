# ESP32-S3 UI – T7 Zusammenfassung

## Überblick

T7 markiert den Abschluss einer stabilen, konsistenten und UX-sauberen Integrationsphase
zwischen **ESP32‑S3 UI (Host)** und **ESP32‑WROOM Powerboard (Client)**.

Ziel von T7 war **keine neue Feature‑Flut**, sondern:
- saubere Zustandsführung
- robuste Kommunikation
- eindeutige visuelle Rückmeldung
- klare Trennung von *Runtime-Logik* und *UI-Darstellung*

Das Ergebnis ist ein **produktionsreifer UI‑Stand**, auf dem weitere Features (T8+) sicher aufbauen können.

---

## Architektur – finaler Stand

### Host ↔ Client Kommunikation
- UART, ASCII‑basiertes Protokoll (CRLF)
- Parsing & Formatting **ausschließlich** in `ProtocolCodec`
- Nicht‑blockierender RX‑Pfad (byteweise, line‑based)
- Einheitlicher Codepfad für UART & TestCases

### Link‑Status / Alive‑Konzept
- `lastRxAnyMs` aktualisiert bei:
  - STATUS
  - ACK
  - PONG
- Alive‑Timeout rein **beobachtend**
- **Safe‑Stop** bei Timeout:
  - SET 0x0000 (alle Aktuatoren aus)
- Link gilt erst nach **LinkSync** (stabile PONG‑Sequenz)

### Single Source of Truth
- **`OvenRuntimeState`**
- UI liest ausschließlich diesen Zustand
- UI schreibt **keine Aktuator‑States**
- Countdown läuft nur in `oven_tick()`

---

## Zustandsmodell

### OvenMode (Runtime)
```cpp
enum class OvenMode : uint8_t {
    STOPPED = 0,
    RUNNING,
    WAITING,
    POST
};
```

### UI‑RunState (abgeleitet)
```cpp
STOPPED → UI STOP
RUNNING → UI RUNNING
WAITING → UI WAIT
POST → UI RUNNING (bewusst)
```

➡️ **POST ist aktiv**, aber kein Sonderfall für Icons.

---

## Screen Main – zentrale Designentscheidungen

### Dial & Preset
- Dial als primäres Zeit‑ und Status‑Element
- Preset‑Box **zentral im Dial**
- Preset‑Box:
  - RUNNING: Grün
  - POST: **Blau**
- Dial‑Frame:
  - RUNNING: Standard
  - POST: **Blau**

➡️ POST ist visuell eindeutig, ohne Icon‑Chaos.

---

### Aktuator‑Icons (AP4.x)

#### Grundprinzip
- Icons zeigen **immer den realen Aktuatorzustand**
- Keine UI‑Simulation (außer WAIT‑Override)

#### Regeln
- STOPPED:
  - alles OFF
- RUNNING:
  - Icons folgen Runtime‑Bits
- POST:
  - **identisch zu RUNNING**
- WAIT:
  - expliziter Safety‑Override:
    - Heater OFF
    - Motor OFF
    - FAN12V ON
    - FAN230_SLOW ON
    - Lamp ON

➡️ **POST hat bewusst keine eigene Icon‑Logik**

---

## WAIT / Door‑Handling

- Tür offen während RUNNING:
  - sofortiger Übergang in WAIT
  - Countdown stoppt
- Resume nur bei:
  - Tür geschlossen
  - explizitem Resume
- Pause‑Button:
  - abhängig von Door‑State & RunState
  - klar visuell blockiert

---

## Start / Stop Logik

- START:
  - nur aus STOPPED
- STOP:
  - aus RUNNING, WAIT, POST
- POST:
  - Start‑Button bleibt **STOP**
  - UI fällt erst nach POST‑Ende auf START zurück

➡️ Verhindert inkonsistente UI‑Zustände (früher Bugfix).

---

## Status‑Icons

### Link‑Icon (AP4.1 / AP4.1b)
- Grün: `linkSynced == true`
- Rot: `linkSynced == false`
- Keine Alive‑Flaps mehr
- Stabil nach Bugfix (`lastRxAnyMs`)

---

## Wichtige Bugfixes & Lessons Learned

### Kommunikations‑Bug
- Ursache: fehlendes Update von `lastRxAnyMs`
- Effekt: Alive‑Timeout trotz gültiger RX‑Frames
- Fix:
  - Update bei **jedem gültigen RX**
  - konsistente Age‑Berechnung

### UI‑Robustheit
- Keine UI‑eigenen Timer für Logik
- UI ist **reiner Renderer**
- Runtime ist autoritativ

---

## Commit‑Konvention (ab T7)

Alle Commits beginnen mit:

```
T7 APx.y – <Kurzbeschreibung>
```

Beispiel:
```
T7 AP4.3 – POST nutzt identische Aktuator‑Iconlogik wie RUNNING
```

---

## Ergebnis

✅ Stabile Host‑Client‑Kommunikation  
✅ Klare UX ohne Sonderfälle  
✅ Wartbarer, erklärbarer Code  
✅ Solide Basis für T8+

**T7 ist abgeschlossen.**
