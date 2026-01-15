# Filament & SilicaGel Dryer – Installation

Diese Anleitung beschreibt die Installation für beide Controller:

- **HOST**: ESP32‑S3 (480×480 Touch‑UI, LVGL 9.4.x)
- **CLIENT**: ESP32‑WROOM (Powerboard‑Controller, Hardware‑I/O)

Es gibt zwei Wege:

1) **Non‑Developer**: Fertige Firmware flashen (HOST + CLIENT)  
2) **Developer**: Projekt aus dem Repo bauen, flashen und debuggen

> Grundannahme: Du hast **Basiswissen** zum Flashen von ESP32 (USB‑Port, Treiber, COM‑Port finden).  
> Ziel hier: **so viel wie nötig, so wenig wie möglich**.

---

## 0) Voraussetzungen (für beide Wege)

### Hardware
- ESP32‑S3 Board (HOST) mit 480×480 RGB‑Panel (ST7701)
- ESP32‑WROOM Board (CLIENT)
- USB‑Kabel (datenfähig)
- Optional: USB‑UART Adapter (falls Board keinen nativen USB hat)

### Software
Wähle **eine** der folgenden Optionen:

- **Option A (empfohlen)**: VS Code + PlatformIO
- **Option B**: `esptool.py` (direkt per CLI)

---

## 1) Non‑Developer: Fertige Firmware flashen (HOST + CLIENT zusammen)

### 1.1 Dateien (Firmware Artefakte)
Du brauchst **zwei** Images (werden vom Projekt bereitgestellt):

- `host_esp32s3.bin` (oder `.hex`, je nach Release-Format)
- `client_esp32wroom.bin`

> Hinweis: Bei ESP32 sind **.bin** üblich. Falls eure Releases „.hex“ heißen, ist das ok – die Flash-Kommandos unterscheiden sich je nach Tool.  
> In den Beispielen unten wird **.bin** verwendet, weil das mit `esptool.py` Standard ist.

---

### 1.2 Flashen mit VS Code + PlatformIO (einfachster Weg)

1. VS Code installieren  
2. Extension **PlatformIO IDE** installieren  
3. Projekt öffnen (oder „PIO Home → Open Project“)  
4. **Board/Environment auswählen** (HOST bzw. CLIENT – je nach Projektstruktur)

Dann:

- Für **HOST**: `Upload` (Pfeil) ausführen  
- Für **CLIENT**: danach Board umstecken und ebenfalls `Upload` ausführen

> Wenn ihr Releases ohne Repo-Build nutzen wollt, ist PlatformIO weniger ideal (weil PIO normalerweise baut). In dem Fall: nutze Option **1.3 esptool.py**.

---

### 1.3 Flashen mit `esptool.py` (CLI, robust, minimal)

#### Installation `esptool`
```bash
python3 -m pip install --user esptool
```

#### Port finden
- macOS: typischerweise `/dev/tty.usbmodem*` oder `/dev/tty.usbserial*`
- Linux: `/dev/ttyACM0` oder `/dev/ttyUSB0`
- Windows: `COM3`, `COM5`, …

Beispiele:
```bash
# macOS / Linux
ls /dev/tty.* 2>/dev/null || true
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
```

---

#### WICHTIG: Flash-Adresse
Für ESP32-Firmware wird sehr häufig an **0x0** geflasht (monolithisches Image).  
Wenn euer Release **mehrere Binaries** enthält (bootloader/partition/table/app), dann müsst ihr die jeweils **korrekten Offsets** verwenden.

✅ **Best-Practice**: Der Release sollte eine `flash_cmd.txt` oder Release-Notes mit Offsets enthalten.

Im Zweifel (und wenn ihr nur **ein** `.bin` habt): **0x0**.

---

#### HOST flashen (ESP32‑S3)
```bash
esptool.py --chip esp32s3 --port /dev/tty.usbmodemXXXX --baud 921600 erase_flash
esptool.py --chip esp32s3 --port /dev/tty.usbmodemXXXX --baud 921600 write_flash -z 0x0 host_esp32s3.bin
```

#### CLIENT flashen (ESP32‑WROOM)
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 erase_flash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash -z 0x0 client_esp32wroom.bin
```

> Wenn der Chip-Typ nicht passt:  
> - WROOM ist meist `esp32` (klassisch)  
> - S3 ist `esp32s3`  
> Falls unsicher: `esptool.py chip_id` testen.

#### Optional: Chip erkennen / Verbindung testen
```bash
esptool.py --port /dev/tty.usbmodemXXXX chip_id
```

---

## 2) Tests / Validierung

### 2.1 Minimaltest ohne CLIENT (HOST allein)
Ohne CLIENT ist das System nur eingeschränkt sinnvoll. Erwartbar ist:

- HOST bootet ohne Crash
- **screen_main** sichtbar
- Swipe zu **screen_config**
- Swipe zu **screen_dbg_hw**
- Keine „echten“ Hardware-Zustände (Telemetrie fehlt)

Das reicht als „Display/Touch/Basic UI“‑Smoke‑Test.

### 2.2 Validierung mit CLIENT (empfohlen)
Wenn HOST↔CLIENT verbunden sind (UART/TTL) und der CLIENT läuft:

- Link/Sync Indicator wird plausibel (je nach Implementierung: „synced“)
- STATUS/ACK‑Traffic sichtbar (falls Logs aktiv)
- `screen_dbg_hw` zeigt reale I/O Zustände und kann (bei aktivem RUN-Gate) toggeln

---

## 3) Developer: Build / Compile / Flash

> Für Entwickler: Ziel ist reproduzierbarer Build via PlatformIO.

### 3.1 Projektsetup (VS Code + PlatformIO)
1. VS Code + PlatformIO installieren  
2. Repo klonen:
```bash
git clone <REPO>
cd <REPO>
```
3. In VS Code öffnen

### 3.2 PlatformIO Environments
Die konkreten Environments stehen in `platformio.ini`.

Typischer Ablauf:
- Environment für **HOST** bauen/flashen
- Environment für **CLIENT** bauen/flashen

Beispiele (CLI):
```bash
# Build
pio run -e host

# Flash
pio run -e host -t upload

# Monitor (optional)
pio device monitor -b 115200
```

Analog für den Client:
```bash
pio run -e client
pio run -e client -t upload
```

> Ersetze `host`/`client` durch eure tatsächlichen Environment-Namen.

---

## 4) Libraries / Abhängigkeiten

Die vollständige Liste ist in `platformio.ini` definiert (Single Source of Truth).

Typisch (HOST):
- LVGL **9.4.x**
- Arduino_GFX_Library (RGB Panel / ST7701 init)
- ggf. Touch-Lib (je nach Panel/Controller)

Typisch (CLIENT):
- UART/Protocol Layer (eigen)
- GPIO/Relais-Treiber (Powerboard)

> Bitte orientiere dich **immer** an `platformio.ini` eures Repos.

---

## 5) Troubleshooting (Kurz)

- **esptool findet Port nicht**: anderes USB-Kabel, Treiber, Port prüfen
- **Schreibfehler / Timeout**: Baudrate reduzieren (z.B. 460800 oder 115200)
- **Boot-Loop**: falsches Board/Chip, falsches Image, falscher Offset
- **Display schwarz**: Backlight-Pin, PCLK, ST7701 Init-Sequenz, RGB/BGR

---

## 6) Safety-Hinweise
- `screen_dbg_hw` ist ein **Testscreeen**.  
  Er darf nur genutzt werden, wenn das System **nicht** im „Running“‑Betrieb ist.
- Bei Navigation/Swipe aus dem Debug-Screen muss ein **Safe‑Off** erzwungen werden (Projekt-Policy).

---

**END OF FILE**
