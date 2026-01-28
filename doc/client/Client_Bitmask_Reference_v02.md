# Client-Bitmasken-Referenz (ESP32-WROOM)
**Version:** v02a  
**Kontext:** Filament-Silicagel-Dryer – Client / ClientComm  
**Geltungsbereich:** Beschreibung der Interpretation von `outputsMask`-Bitmasken und der zugehörigen physischen Ausgänge.

---

## 1. Überblick

Dieses Dokument beschreibt, wie der **CLIENT (ESP32-WROOM)** Bitmasken interpretiert, die vom **HOST** über das UART-Protokoll gesendet werden.

- Der HOST sendet Befehle (`SET`, `UPD`, `TOG`) mit einer **16-Bit-Maske**
- Der CLIENT:
  - speichert die Maske intern (`_outputsMask`)
  - setzt die Hardware-Ausgänge entsprechend
  - meldet den aktuellen Zustand zyklisch per `STATUS`

**Wichtig:**  
Der CLIENT ist **hardware-autoritativ**. Die UI auf dem HOST stellt ausschließlich den vom CLIENT gemeldeten Ist-Zustand dar.

---

## 2. Grundprinzip der Bitmasken

- Jedes Bit der Maske repräsentiert **einen logischen Aktor oder Zustand**
- Bit = `1` → Aktor EIN / Zustand aktiv
- Bit = `0` → Aktor AUS / Zustand inaktiv

Die Bedeutung der Bits ist **fest definiert** und darf zwischen HOST und CLIENT nicht variieren.

---

## 3. Bit-Definitionen

### 3.1 Übersicht der Bits

| Bit | Maske (hex) | Logischer Name | Bedeutung |
|----:|:-----------:|---------------|----------|
| 0 | 0x0001 | FAN12V | 12V-Lüfter EIN |
| 1 | 0x0002 | FAN230V | 230V-Lüfter (schnell) |
| 2 | 0x0004 | FAN230V_SLOW | 230V-Lüfter (langsam) |
| 3 | 0x0008 | SILICAT_MOTOR | Silikagel-Motor |
| 4 | 0x0010 | HEATER | Heizung aktiv |
| 5 | 0x0020 | LAMP | Innenbeleuchtung |
| 6 | 0x0040 | DOOR_ACTIVE | Türsignal aktiv |

**Hinweise:**
- `DOOR_ACTIVE` ist ein **Sensorsignal**, kein schaltbarer Ausgang
- Bits oberhalb von Bit 6 sind aktuell **reserviert**

---

### 3.2 Logische Namen ↔ GPIO-Zuordnung (ESP32-WROOM)

Erweiterung gegenüber v01:  
Diese Tabelle ergänzt die logischen Bitnamen um die **konkreten GPIO-Pins** des ESP32-WROOM.

| Bit | Maske (hex) | Logischer Name | GPIO | Richtung | Beschreibung |
|----:|:-----------:|---------------|-----:|:--------:|-------------|
| 0 | 0x0001 | FAN12V | 32 | OUT | 12V-Kühlung (Board) |
| 1 | 0x0002 | FAN230V | 33 | OUT | Lüfter hohe Stufe |
| 2 | 0x0004 | FAN230V_SLOW | 27 | OUT | Lüfter niedrige Stufe |
| 3 | 0x0008 | SILICAT_MOTOR | 26 | OUT | Silikagel-Trommel |
| 4 | 0x0010 | HEATER | 12 | OUT (PWM) | Heizung (PWM) |
| 5 | 0x0020 | LAMP | 25 | OUT | Innenbeleuchtung |
| 6 | 0x0040 | DOOR_ACTIVE | 14 | IN | Tür offen / aktiv |

---

## 4. Sicherheitslogik (Zusammenfassung)

- `DOOR_ACTIVE = 1` bedeutet: **Tür offen**
- Bei aktiver Tür müssen folgende Aktoren **deaktiviert** sein:
  - HEATER
  - SILICAT_MOTOR
- Diese Logik wird:
  - hardwareseitig (Inhibit)
  - softwareseitig (Policy)
  umgesetzt
- Die **Lampe** ist bewusst nicht türabhängig

---

## 5. STATUS-Telemetrie

Der CLIENT sendet regelmäßig `STATUS`-Frames an den HOST.

Ein STATUS enthält u. a.:
- aktuelle `outputsMask`
- Türzustand
- Temperaturdaten (ADC / Thermoelement)

Die UI darf **keine eigenen Zustände erfinden**, sondern rendert ausschließlich diese Daten.

---

## Anhang A – GPIO-Zuordnungen aus `pins_client.h`

### A.1 Logische Ofen-Signale → GPIO

| Logische Konstante | GPIO | Quelle |
|------------------|-----:|--------|
| OVEN_FAN12V | 32 | PIN_CH0 |
| OVEN_FAN230V | 33 | PIN_CH1 |
| OVEN_FAN230V_SLOW | 27 | PIN_CH4 |
| OVEN_LAMP | 25 | PIN_CH2 |
| OVEN_SILICAT_MOTOR | 26 | PIN_CH3 |
| OVEN_DOOR_SENSOR | 14 | PIN_CH5 |
| OVEN_HEATER | 12 | PIN_CH6 |

---

### A.2 Digitale Kanal-Pins

| Pin-Konstante | GPIO |
|--------------|-----:|
| PIN_CH0 | 32 |
| PIN_CH1 | 33 |
| PIN_CH2 | 25 |
| PIN_CH3 | 26 |
| PIN_CH4 | 27 |
| PIN_CH5 | 14 |
| PIN_CH6 | 12 |
| PIN_CH7 | 13 |
| PIN_CH8 | 4 |
| PIN_CH9 | 0 |
| PIN_CH10 | 2 |
| PIN_CH11 | 15 |

---

### A.3 Analoge Eingänge

| Pin-Konstante | GPIO |
|--------------|-----:|
| PIN_ADC0 | 36 |
| PIN_ADC1 | 39 |
| PIN_ADC2 | 34 |
| PIN_ADC3 | 35 |

---

### A.4 Thermoelement (MAX6675)

| Signal | GPIO |
|-------|-----:|
| SCK | 18 |
| CS | 5 |
| SO | 19 |

---

### A.5 UART (Host ↔ Client)

| Signal | GPIO |
|-------|-----:|
| CLIENT_RX2 | 16 |
| CLIENT_TX2 | 17 |

---

**Ende des Dokuments**
