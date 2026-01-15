
# Filament-SilicaGel-Dryer – Installation Guide

Diese Anleitung beschreibt die Installation des Filament- und SilicaGel-Dryers
für **Non-Developer** und **Developer**.  
Die Installation unterscheidet sich in **HOST (ESP32-S3)** und **CLIENT (ESP32-WROOM)**,
wird für Non-Developer jedoch **gemeinsam** durchgeführt.

---

## Zielgruppen

- **Non-Developer**  
  Anwender ohne Entwicklungsumgebung, die ausschließlich fertige Firmware flashen.

- **Developer**  
  Anwender, die Firmware selbst bauen, anpassen oder erweitern möchten.

---

# NON-DEVELOPER INSTALLATION (Empfohlen)

## Überblick

Für Non-Developer wird **HOST und CLIENT gemeinsam geflasht**.  
Dies reduziert Fehlerquellen und vereinfacht die Inbetriebnahme erheblich.

### Voraussetzungen
- PC / Mac / Linux
- USB-Kabel
- Flash-Tool (z. B. esptool, vendor tool)
- Fertige HEX-/BIN-Firmware für:
  - ESP32-S3 (HOST)
  - ESP32-WROOM (CLIENT)

---

## Schritt 1 – Flash HOST & CLIENT

1. ESP32-S3 (HOST) per USB verbinden
2. HOST-Firmware flashen
3. ESP32-WROOM (CLIENT) per USB/UART verbinden
4. CLIENT-Firmware flashen
5. Beide Boards spannungsfrei machen
6. Gesamtsystem verbinden und einschalten

---

## Schritt 2 – Minimaler Funktionstest (ohne Client-Validierung)

> Ein vollständiger Test **ohne Client ist nicht sinnvoll** und daher bewusst reduziert.

Erwartetes Verhalten nach dem Einschalten:

- Display bootet
- **screen_main** sichtbar
- Swipe zu **screen_config** funktioniert
- Swipe zu **screen_dbg_hw** funktioniert
- Keine Abstürze / kein Reboot

⚠️ Aktoren, Temperaturen und Hardware-Status sind ohne Client **nicht aussagekräftig**.

---

# DEVELOPER INSTALLATION

## Überblick

Für Entwickler werden HOST und CLIENT **getrennt** behandelt,
inklusive Build-, Compile- und Flash-Schritten.

---

## HOST – ESP32-S3 (UI / Controller)

### Voraussetzungen
- Git
- PlatformIO
- VS Code (empfohlen)
- USB-Treiber für ESP32-S3

### Projekt-Setup
```bash
git clone <repository-url>
cd filament-silicagel-dryer
```

### Build & Flash
```bash
pio run -e esp32s3 -t upload
```

---

### Relevante Libraries (HOST)

- LVGL 9.4.x
- Arduino_GFX_Library
- MAX31856 / MAX6665 (K-Type Sensor)
- Custom UI / Oven Logic Modules

(Details siehe `platformio.ini`)

---

## CLIENT – ESP32-WROOM (Powerboard Controller)

### Build & Flash
```bash
pio run -e esp32wroom -t upload
```

### Funktion
- Aktorensteuerung (Heater, Fans, Motor, Lamp)
- Sensorauswertung (Door, Temperature)
- UART-Protokoll zur HOST-Kommunikation

---

## Systemtest (Developer)

Nach erfolgreichem Flash von HOST & CLIENT:

### Erwartete Log-Ausgaben (HOST)
- Display Init OK
- LVGL Init OK
- UART Link Sync
- STATUS Frames empfangen

### Erwartete Log-Ausgaben (CLIENT)
- Boot OK
- UART Ready
- STATUS Frames gesendet

---

## Typische Fehlerbilder

| Symptom | Ursache |
|-------|--------|
| Schwarzer Screen | Display-Init / Backlight |
| Falsche Farben | RGB/BGR Mismatch |
| Keine Aktoren | UART / Client nicht aktiv |
| Reboots | Versorgung / PCLK zu hoch |

---

## Hinweise

- Ohne CLIENT ist nur UI-Navigation testbar
- Safety-Mechanismen greifen bei Link-Verlust automatisch
- screen_dbg_hw ist **nur für Entwicklung/Test**

---

**Ende der Installationsanleitung**
