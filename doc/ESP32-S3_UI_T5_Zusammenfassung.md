# ESP32-S3 UI – T5 Zusammenfassung

## Projektkontext
- **Plattform:** ESP32-S3  
- **Display:** 480×480  
- **UI-Framework:** LVGL 9.x  
- **Projekt:** Filament Dryer UI  

## Screens
- `screen_main` – Laufzeit-Anzeige & Steuerung  
- `screen_config` – Preset-Auswahl & Runtime-Anpassung  
- `screen_log` – Status/Logs  

---

## Architektur – Kernprinzipien (T5)

### Single Source of Truth
- **Ofen-Logik ist autoritativ**
- UI rendert **ausschließlich** aus:
  ```cpp
  oven_get_runtime_state()
  ```

### Zeitverwaltung
- **Countdown läuft nur im Ofen**
  - `oven_tick()` @ 1 Hz
- UI zählt **keine Zeit selbst**
- Keine konkurrierenden Timer mehr

> **Regel:**  
> *UI zeigt Zeit an – Ofen zählt Zeit.*

---

## Preset- & Runtime-Modell

### Presets
- `kPresets[]` sind **immutable**
- Enthalten Werkseinstellungen (Zeit, Temperatur, Rotary)

### Runtime
- User darf:
  - Zeit (HH/MM)
  - Temperatur
  anpassen
- Änderungen gelten **nur für die aktuelle Session**
- Speicherung in `runtimeState`
- `currentProfile` wird beim Start **nicht überschrieben**

---

## Screen-Wechsel: Config → Main

### Ablauf
1. User passt Preset-Zeit / Temperatur an
2. Werte werden in `runtimeState` übernommen
3. Wechsel zu `screen_main`
4. Direktes Refresh:
   ```cpp
   screen_main_refresh_from_runtime();
   ```

### Ergebnis
- Needles
- Remaining Time
- Progressbar
- Preset-Box  
sind **sofort korrekt**

---

## Needles (Dial)

### Implementierung
- `lv_line` mit **mutable points**
- Keine PNGs
- Zentrale Methode:
  ```cpp
  set_needles_hms(hh, mm, ss);
  ```

### Aktualisierung
- Ausschließlich über Runtime-State
- Keine UI-eigene Logik

---

## Countdown-Logik (Final)

### Entfernt
- `countdown_tick_cb()` als Zeitquelle
- `g_remaining_seconds--` im UI

### Aktiv
- `oven_tick()` reduziert `runtimeState.secondsRemaining`
- UI liest nur noch diesen Wert

---

## START / STOP / WAIT State Machine

### Zustände
```cpp
enum class RunState {
    STOPPED,
    RUNNING,
    WAIT
};
```

### START
- `oven_start()`
- Runtime-Werte werden übernommen
- UI startet nur die Anzeige

### STOP
- `oven_stop()`
- UI setzt Anzeige zurück

### WAIT
- Countdown pausiert
- Aktuator-Icons zeigen Safe-State
- Resume nur bei geschlossener Tür

---

## UI-Komponenten (Status T5)

### Main Screen
- Dial mit HH/MM/SS-Needles
- Preset-Box im Zentrum:
  - dynamische Font-Anpassung
  - Truncation + Ellipsis
- Temperatur-Skala:
  - Target / Current Marker
  - Farbstatus (cold / ok / hot)
- Aktuator-Icons:
  - Recolor
  - Heater-Pulse
  - WAIT-Override
- START / STOP / WAIT Buttons stabil

### Config Screen
- Filament / Time / Temp Cards
- Runtime-Anpassung stabil
- SAVE blockiert bei RUNNING

---

## Bekannte Grenzen (bewusst offen für T6)

- Keine Persistenz (NVS)
- UI-Feinschliff offen
- UX-Verbesserungen (WAIT/Resume)
- Update-Takt-Optimierung

---

## Fazit T5

- Architektur stabilisiert
- Zeitlogik korrekt entkoppelt
- UI deterministisch
- Solide Basis für **T6**
