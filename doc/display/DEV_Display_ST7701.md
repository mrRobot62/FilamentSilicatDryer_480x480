# Display-Integration und Initialisierung (ESP32-S3 + ST7701)

## Ziel dieses Dokuments
Diese Entwickler-Dokumentation beschreibt ausschließlich die Display-Ansteuerung des Projekts:
- Hardware-Anbindung des 480×480 RGB-Panels HSD040BPN1
- Initialisierung des ST7701 Controllers
- Farbraum- und Pixelformat-Probleme (RGB/BGR, 565/666)
- Lessons Learned und typische Fehlerquellen

Der Aufwand und die Komplexität dieses Teils rechtfertigen eine eigene, dedizierte Dokumentation.

---

## Hardware-Überblick

ESP32-S3  
- RGB-Parallel Interface (16 Bit)  
- Timing-Signale (PCLK, HSYNC, VSYNC, DE)  
- SWSPI nur für ST7701-Register  
- Separater Backlight-GPIO

---

## Architektur der Display-Initialisierung

init_display()  
- Arduino_SWSPI  
- Arduino_ESP32RGBPanel  
- Arduino_RGB_Display  
- gfx->begin(PCLK)  
- Backlight enable

---

## Pixelclock und Stabilität

Erprobte Ergebnisse:
- 16 MHz instabil nach längerer Laufzeit
- 12 MHz stabil (empfohlen)

---

## Farbdarstellung – kritischer Punkt

- RGB565 laut Datenblatt unbrauchbar
- RGB666 liefert korrekte Farben
- BGR-Farbreihenfolge notwendig

Typische Fehler:
- Weiß wird gelb
- Rot und Blau vertauscht
- Farbstich in Graustufen

---

## Initialisierungs-Reihenfolge

1. Register-Setup
2. Gamma und Power
3. Timing (TCON)
4. Pixelformat
5. Sleep Out
6. Display On

---

## Fazit

Die Display-Integration war einer der aufwendigsten Teile des Projekts.
Die dokumentierte Lösung ist stabil, reproduzierbar und panel-spezifisch.
