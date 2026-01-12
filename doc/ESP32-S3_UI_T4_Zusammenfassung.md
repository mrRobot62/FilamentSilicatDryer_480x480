# ESP32-S3 UI – Fortsetzung T4 (Abschlusszusammenfassung)

## Überblick

Diese Datei dokumentiert den **finalen Abschlussstand von T4** der ESP32‑S3 UI‑Entwicklung.
Der Fokus von T4 lag vollständig auf der **Implementierung und Stabilisierung von `screen_main`**.
Die Architektur, UI‑Logik und Zustandsführung gelten nach diesem Stand als **abgeschlossen und belastbar**.

Diese Zusammenfassung dient als **technische Referenz** für spätere Erweiterungen (insb. T5: `screen_config`).

---

## Technische Basis

- **Hardware:** ESP32‑S3  
- **Display:** 480×480  
- **UI Framework:** LVGL 9.x  
- **Sprache:**  
  - Kommunikation: Deutsch  
  - Sourcecode & Inline‑Kommentare: Englisch  
- **Build‑Status:** Compile & Flash OK

---

## Architektur‑Grundsätze

### Single Source of Truth

- **`oven.cpp / oven.h`** sind die **einzige Quelle der Wahrheit**
- Die UI **setzt keinen eigenen Zustand**
- UI rendert **ausschließlich** aus `OvenRuntimeState`

```text
oven_tick()
   ↓
OvenRuntimeState
   ↓
screen_main_update_runtime()
   ↓
UI (reines Rendering)
```

### Preset vs. Runtime

| Ebene | Bedeutung |
|-----|----------|
| Preset | Werkseinstellungen (Name, Basis‑Temp, Dauer, Motor‑Flag) |
| Runtime | Laufzeitwerte (Temp, Zeit, Remaining, WAIT, RUNNING) |

- Presets sind **immutable**
- Runtime‑Werte sind **temporär**
- **Keine Persistenz** in T4 (NVS ist Zukunftsthema)

---

## Preset‑System

### Werkspresets

```cpp
static constexpr FilamentPreset kPresets[];
```

- Enthält alle vordefinierten Filamentprofile
- Enthält u. a.:
  - Name
  - Zieltemperatur
  - Dauer
  - Rotary/Motor‑Flag

### Default‑Preset

```cpp
#define OVEN_DEFAULT_PRESET_INDEX 5   // PLA
```

- Wird **explizit in `oven_init()` gesetzt**
- Wichtig: `oven_init()` **muss aktiv aufgerufen werden**
- Initialisiert:
  - `presetName`
  - `filamentId`
  - `durationMinutes`
  - `secondsRemaining`
  - `tempTarget`
  - `rotaryOn`

---

## screen_main – UI‑Aufbau

### Layout‑Struktur

```text
┌────────────────────────────┐
│ Topbar                     │
│ ─ Progressbar              │
│ ─ Remaining Time           │
├────────────────────────────┤
│ Center                     │
│ ├ Icons (links)            │
│ ├ Dial (Mitte)             │
│ │ ├ Hour/Minute/Second     │
│ │ ├ Preset‑Box (Overlay)   │
│ ├ Start / Pause (rechts)   │
├────────────────────────────┤
│ Page Indicator             │
├────────────────────────────┤
│ Bottom                     │
│ ─ Temperatur‑Skala         │
│ ─ Target / Current Marker  │
└────────────────────────────┘
```

---

## Dial & Countdown

- Dial basiert auf `lv_scale`
- Zeiger:
  - Hour
  - Minute
  - Second
- Implementiert mit `lv_line` + **mutable points**
- Countdown läuft **ausschließlich im UI‑Timer**
- Oven‑Logik bleibt davon entkoppelt

---

## Preset‑Box (Dial‑Zentrum)

- Eigenes Objekt auf `ui.root` → liegt **über den Needles**
- Abgerundete Box, grün, weißer Rand
- Zweizeilig:
  - **PresetName**
  - **#<filamentId>** (gedimmt)

### Wichtige Design‑Entscheidung

- Filamentnamen werden **bewusst begrenzt**
- Keine automatische Skalierung / Ellipsis‑Magie mehr
- Presets sind fest → Namen können kontrolliert kurz gehalten werden
- Ergebnis: robuster Code, weniger Edge‑Cases

---

## Aktuator‑Icons

- Weiße PNG‑Icons
- Laufzeit‑Recolor per `img_recolor`
- Semantik:
  - OFF → weiß
  - ON → grün
  - Door open → rot
- Icons sind **Statusanzeige**, keine Logik

### Manuelle Interaktion

- **LAMP:** jederzeit manuell schaltbar
- **FAN230:** manuell schaltbar, wenn Ofen nicht läuft (Cooldown)

---

## Start / Stop / WAIT

### RunState

```cpp
enum class RunState {
    STOPPED,
    RUNNING,
    WAIT
};
```

### Verhalten

| Zustand | START‑Button | PAUSE‑Button |
|------|-------------|-------------|
| STOPPED | START (orange) | disabled |
| RUNNING | STOP (rot) | PAUSE |
| WAIT | STOP (rot) | WAIT / Resume (door‑abhängig) |

- WAIT snapshotet den Zustand
- Resume nur bei geschlossener Tür
- Tür offen → WAIT blockiert

---

## Temperatur‑Anzeige

- Horizontale Skala (0–120 °C)
- Marker:
  - Target (unten, ▲)
  - Current (oben, ▼)
- Farben:
  - Cold → Blau
  - OK → Grün
  - Hot → Orange
- Target‑Label wird gedimmt, wenn Temperatur im Toleranzband liegt

---

## Debug & Sicherheit

- Simulierter Door‑Override im UI möglich
- Türöffnung während RUNNING:
  - Sofortiger WAIT
  - Countdown stoppt
- UI erzwingt sichere Zustände visuell

---

## Abschlussstatus T4

✔ screen_main vollständig implementiert  
✔ Architektur sauber getrennt  
✔ Compile & Flash stabil  
✔ Keine offenen funktionalen TODOs  

**T4 gilt als abgeschlossen.**

---

## Ausblick T5

Nächster Schritt:
**screen_config**

Geplant:
- Preset auswählen
- Zeit & Temperatur anpassen
- Änderungen wirken nur auf Runtime
- Keine Persistenz (NVS folgt später)

---

_Ende der T4‑Zusammenfassung_
