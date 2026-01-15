# ESP32-S3 UI – Fortsetzung T3  
**Zusammenfassung & Abschlussdokumentation**

Diese Datei fasst den Entwicklungsstand des UI-Projekts **ESP32-S3 UI – Fortsetzung T3** zusammen.  
T3 gilt als **abgeschlossen** und dient als Referenz- und Übergabestand für **T4**.

---

## 1. Projektübersicht

- **Plattform:** ESP32-S3 (Host / UI)
- **Display:** 480×480 Touch-Display
- **GUI-Framework:** LVGL
- **Architektur:**  
  - UI (ESP32-S3) vollständig **hardware-agnostisch**  
  - Hardwaresteuerung ausgelagert auf **ESP32-WROOM (Client)**  
  - Kommunikation ausschließlich über **Serial2** (UART)

---

## 2. Main-Screen Layout (Final T3)

### Geometrie (480×480)

```
+------------------------------------------------+
| Topbar: Time Progress (360px)        HH:MM     |
|                                                |
| [Icons]   +------------------------+   [BTN]  |
|           |        Dial (300x300)   |          |
|           |  - Filament Label       |  START   |
|           |  - Remaining HH:MM:SS   | / STOP   |
|           +------------------------+          |
|                                                |
|           Page Indicator (● ○ ○)               |
|                                                |
| Temp Scale 0–120°C (360px)      Current °C     |
+------------------------------------------------+
```

### Elemente im Detail

- **Topbar**
  - Zeit-Fortschrittsbalken (360 px, Padding 60 px links/rechts)
  - Restzeit HH:MM rechts

- **Center / Dial**
  - Kreisförmige Countdown-Darstellung (300×300)
  - Filament-Label im Zentrum
  - Restzeit HH:MM:SS im Dial

- **Left Column – Status Icons**
  - fan12v
  - fan230
  - fan230_slow
  - heater
  - door
  - motor
  - lamp

- **Right Column**
  - Quadratischer Start/Stop-Button

- **Page Indicator**
  - 3 Punkte: main / config / log
  - Aktive Seite hervorgehoben

- **Bottom**
  - Temperatur-Skala 0–120 °C
  - Soll-Marker
  - Ist-Temperatur-Indikator + Label

---

## 3. Icon-System

### Grundprinzip

- Alle Icons sind **weiß** designt (PNG mit Alpha)
- Farbänderung erfolgt ausschließlich über **LVGL img_recolor**

### Farb-Logik

| Zustand    | Farbe |
| ---------- | ----- |
| OFF        | Weiß  |
| ON         | Grün  |
| Door offen | Rot   |

### Technische Umsetzung

```cpp
lv_obj_set_style_img_recolor(icon, color, LV_PART_MAIN);
lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
```

OFF-Zustand:
```cpp
lv_obj_set_style_img_recolor_opa(icon, LV_OPA_TRANSP, LV_PART_MAIN);
```

---

## 4. Start / Stop Button

### Design-Entscheidungen

- LVGL-Theme **vollständig entfernt**
- Keine Gradients, keine Theme-Overrides

```cpp
lv_obj_remove_style_all(ui.btn_start);
```

### Zustandslogik

| Zustand      | Farbe  | Label |
| ------------ | ------ | ----- |
| OVEN_STOPPED | Orange | START |
| OVEN_RUNNING | Rot    | STOP  |

### Begründung

- Volle Kontrolle über Darstellung
- Keine Seiteneffekte durch Theme
- Deterministisches Verhalten

---

## 5. Dial Needles (Zeiger)

### Architekturentscheidung

- **Keine lv_line mehr**
- Zeiger sind **Icons (lv_image)**

### Gründe

- Keine Layout-Interferenzen
- Saubere Rotation
- Klare Kontrolle über Pivot & Radius
- Stabil (keine 1/3–2/3 Rendering-Probleme)

### Parent

- Alle Needles sind **Child von `ui.root`**
- `LV_OBJ_FLAG_IGNORE_LAYOUT` gesetzt

### Pivot & Positionierung

- Pivot: **unten-mittig**
- Align: Dial-Zentrum **mit Offset-Korrektur**

```cpp
pivot_x = width / 2
pivot_y = height - 1

offset_y = (height / 2) - pivot_y
```

### Radius / Länge

- Needle-Länge wird **im PNG definiert**
- Beispiel:
  - Minute: ~145 px
  - Stunde: ~120 px (≈ 25 px kürzer)

### Farbkonzept

| Needle                 | Farbe              |
| ---------------------- | ------------------ |
| Minute (MM)            | Orange             |
| Hour (HH)              | Weiß               |
| (Optional) Second (SS) | Grau / Akzentfarbe |

---

## 6. Aktueller Stand nach T3

### Was funktioniert stabil

- Gesamtes Main-Screen-Layout
- Touch & Events
- Start/Stop Logik
- Icon-Farblogik
- Dial-Grunddarstellung
- Needle-Darstellung & Zentrierung

### Noch **nicht** Bestandteil von T3

- Needle-Rotation (Countdown-Logik)
- Konfigurations-Screen
- Log-Screen
- Persistenz (NVS)
- Erweiterte Animationen

---

## 7. Grundlegendes Architekturprinzip
```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                            Developer Architecture                            │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────┐        ┌──────────────────────────────┐    │
│  │        UI Layer (LVGL)       │        │     Hardware / Powerboard    │    │
│  │                              │        │                              │    │
│  │  ┌────────────────────────┐  │        │  ┌──────────────────────────┐│    │
│  │  │ screen_main             │ │        │  │ Heater / Fans / Motor    ││    │
│  │  │ screen_config           │ │        │  │ Relays / Door / Lamp     ││    │
│  │  │ screen_dbg_hw (Safety)  │ │        │  └──────────────────────────┘│    │
│  │  │ screen_log (Placeholder)│ │        │                              │    │
│  │  └────────────────────────┘  │        │        ESP32-WROOM           │    │
│  │                              │        │                              │    │
│  │  UI = Rendering only         │        │  Executes commands           │    │
│  │  No business logic           │        │  Reports real HW state       │    │
│  └───────────────▲──────────────┘        └───────────────▲──────────────┘    │
│                  │                                       │                   │
│                  │        UART / TTL (ASCII Protocol)    │                   │
│                  └───────────────────────────────────────┘                   │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                         Single Source of Truth (Core)                        │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                                oven                                    │  │
│  │                                                                        │  │
│  │  ┌──────────────────────────────────────────────────────────────────┐  │  │
│  │  │ OvenRuntimeState                                                 │  │  │
│  │  │                                                                  │  │  │
│  │  │  running / waiting / stopped                                     │  │  │
│  │  │  duration / remaining time                                       │  │  │
│  │  │  tempCurrent / tempTarget                                        │  │  │
│  │  │  outputs mask (fan, heater, motor, lamp, door)                   │  │  │
│  │  │  linkSynced / commAlive                                          │  │  │
│  │  └──────────────────────────────────────────────────────────────────┘  │  │
│  │                                                                        │  │
│  │  oven_tick()                                                           │  │
│  │  ─ countdown                                                           │  │
│  │  ─ state transitions                                                   │  │
│  │  ─ safety decisions                                                    │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                              Update Flow (Runtime)                           │
│                                                                              │
│  oven_tick()                                                                 │
│      ↓                                                                       │
│  OvenRuntimeState (single source of truth)                                   │
│      ↓                                                                       │
│  screen_*_update_runtime()                                                   │
│      ↓                                                                       │
│  UI Rendering (no logic, no decisions)                                       │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘

```


## 8. Übergang zu T4

### Ziel von T4

- Weiterentwicklung des Main-Screens
- Implementierung der Needle-Rotation (Countdown, rückwärts)
- UI ↔ Serial2 Command-Mapping
- Runtime-State-Update aus Client-Status
- Feinschliff & Stabilisierung

### Wichtig

T3 dient als **stabile visuelle und architektonische Basis**.  
Alle Entscheidungen in T3 sind bewusst getroffen und gelten als **gesetzt**.

---

## 9. Fazit

T3 hat folgende Kernziele erfolgreich erreicht:

- saubere UI-Architektur
- klare Trennung UI ↔ Hardware
- stabile LVGL-Nutzung ohne Seiteneffekte
- erweiterbare Basis für T4

➡️ **T3 abgeschlossen.**
