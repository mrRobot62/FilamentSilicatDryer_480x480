# ESP32-S3 UI – T8 Abschluss-Zusammenfassung

## Überblick
T8 erweitert das bestehende, stabile T7-System um einen **temporären Debug-Hardware-Screen (`screen_dbg_hw`)**.
Dieser Screen dient **ausschließlich Entwicklungs- und Testzwecken** und erlaubt das gezielte Schalten einzelner Hardware-Ports unter klaren Sicherheitsregeln.

Der produktive Betrieb (RUNNING / POST) bleibt davon vollständig unberührt.

---

## Architektur-Prinzipien (unverändert aus T7)
- ESP32-S3 (UI/Host) ↔ ESP32-WROOM (Powerboard/Client) via UART
- Line-basiertes ASCII-Protokoll mit CRLF
- **Single Source of Truth:** `oven.cpp` / `OvenRuntimeState`
- UI liest ausschließlich RuntimeState (kein Shadow-State)
- Aktuator-Zustände kommen **nur** vom Client (STATUS / ACK)
- `oven_tick()` ist allein für Zeit/Countdown zuständig
- Link-Sync über PING/PONG
- Safe-Stop bei Alive-Timeout
- Keine Persistenz (Runtime-only)

---

## Neuer Screen: `screen_dbg_hw`

### Zweck
- Manuelles Schalten einzelner Hardware-Ausgänge
- Test von Verkabelung, Relais, Lüftern, Heizung etc.
- Explizit **nicht** für den normalen Betrieb gedacht

### Ports / Reihenfolge
1. FAN12V  
2. FAN230V  
3. FAN230V_SLOW  
4. HEATER  
5. DOOR (Input / Anzeige, **nicht schaltbar**)  
6. MOTOR  
7. LAMP  

---

## UI-Struktur

### Left (Icons)
- 7 Icons, vertikal angeordnet
- Farbe:
  - Weiß = OFF
  - Grün = ON
  - Door: Rot bei `door_open`

### Middle (Rows)
- 7 Rows, je Icon eine Zeile
- Ganze Row klickbar (nicht nur Icon)
- Row-Hintergrund + Rahmen folgen Icon-Farbe
- Text minimal aus RuntimeState:
  - `PORTNAME: ON|OFF`

### Verbindungslinien
- Abgewinkelte Linien (lv_line) von Icon → Row
- Farbe synchron zum Port-Zustand
- Umsetzung:
  - absolute Koordinaten → lokal auf Line-Objekt gemappt
  - `lv_point_precise_t`
  - Layout-Update vor Berechnung (`lv_obj_update_layout()`)

### Right (Buttons)
- **RUN**
  - Orange = disarmed
  - Rot = armed
  - Nur bei RUN=true dürfen Toggles ausgelöst werden
- **CLEAR / ALL OFF**
  - Schaltet **alle Outputs aus**
  - Setzt RUN wieder auf false
  - Löscht Content-Texte

### Bottom
- Temperatur-Skala identisch zu `screen_main`
- Target / Current Marker + Labels
- Reine Anzeige (keine Logik)

### Page Indicator
- Einheitliche Implementierung (Flex-Layout)
- 4 Dots (MAIN, CONFIG, DBG_HW, LOG)

---

## Zentrale Safety-Regeln (T8)

### RUN-Gate
- Kein Port-Toggle ohne RUN=true
- RUN ist **reine UI-Sicherheit**, kein Oven-State

### CLEAR / ALL OFF
- Führt immer zu:
  - `oven_force_outputs_off()`
  - RUN=false
  - UI-Reset

### Swipe-Safety (kritisch)
- **Jeder Swipe aus `screen_dbg_hw` heraus**:
  - erzwingt Outputs OFF
  - setzt RUN auf false
- Implementiert **zentral im `screen_manager.cpp`**
- Grund: kein bewaffneter Debug-Zustand darf Screen-übergreifend bestehen bleiben

---

## Oven-Core Erweiterung (minimal)

### Neue zentrale Funktion
```cpp
void oven_force_outputs_off(void);
```

Eigenschaften:
- `comm_send_mask(0x0000)`
- setzt `g_remoteOutputsMask = 0`
- verändert **keine** Modi, Zeiten oder State-Maschinen
- wird genutzt von:
  - CLEAR-Button
  - Swipe-Safety
  - ggf. später Link-Loss / Watchdog

➡️ **Einheitliche Safety-API, keine Debug-Sonderwege**

---

## Screen-Manager Anpassungen
- `SCREEN_DBG_HW` nur erreichbar, wenn **nicht RUNNING**
- Swipe-Erkennung bleibt zentral
- Vor Screenwechsel:
  - Prüfung: aktueller Screen == DBG_HW
  - dann Safety-Reset auslösen

---

## Wichtige Erkenntnisse / Lessons Learned
- Debug-Screens brauchen **stärkere Sicherheitsmechanismen** als normale UI
- Navigation ist ein **kritischer Übergang**, dort gehört Safety hin
- „Single Source of Truth“ bleibt auch im Debug-Fall unangetastet
- Größere Touch-Zonen (Rows statt Icons) sind im Test-UI sinnvoll
- Redundante APIs (dbg_clear vs. force_off) konsequent zusammenführen

---

## Status T8
- ✅ Kompiliert & flashbar
- ✅ Stabil im Betrieb
- ✅ Keine Regressionen zu T7
- ✅ DBG_HW sauber isoliert
- ✅ Safety-Regeln vollständig umgesetzt

---

**T8 gilt damit als abgeschlossen.**
