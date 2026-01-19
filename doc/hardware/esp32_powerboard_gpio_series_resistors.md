# ESP32-WROOM ↔ PowerBoard – GPIO-Serienwiderstände (220–330 Ω)

Dieses Dokument beschreibt **vollständig und detailliert**, welche GPIO-Pins des ESP32-WROOM
mit **Serienwiderständen (typisch 220–330 Ω)** an das PowerBoard angebunden werden sollen
und **warum** dies empfohlen ist.

Grundlage ist das von dir bereitgestellte GPIO-/Channel-Mapping.

---

## 1) Empfohlene Serienwiderstände (Hauptsignale: Aktoren & UART)

| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| **OVEN_FAN12V** | **GPIO32** | **PIN_CH0** | **220–330 Ω** | Digitaler Ausgang zum PowerBoard-Treiberkanal (CH0). Serienwiderstand empfohlen (Schutz & EMV). |
| **OVEN_FAN230V** | **GPIO33** | **PIN_CH1** | **220–330 Ω** | Digitaler Ausgang (CH1) zum TRIAC-/Treiberpfad. Serienwiderstand empfohlen. |
| **OVEN_FAN230V_SLOW** | **GPIO25** | **PIN_CH2** | **220–330 Ω** | Digitaler Ausgang (CH2) zum TRIAC-/Treiberpfad. Serienwiderstand empfohlen. |
| **OVEN_LAMP** | **GPIO26** | **PIN_CH3** | **220–330 Ω** | Digitaler Ausgang (CH3) zum Treiberpfad. Serienwiderstand empfohlen. |
| **OVEN_SILICAT_MOTOR** | **GPIO27** | **PIN_CH4** | **220–330 Ω** | Digitaler Ausgang (CH4) zum TRIAC-/Motor-Treiberpfad. Serienwiderstand empfohlen. |
| **OVEN_HEATER** | **GPIO12** | **PIN_CH6** | **220–330 Ω** | Digitaler Ausgang (CH6) zum Relais-Treiberpfad (Heater). Serienwiderstand empfohlen. |
| **UART_TX (Client → PowerBoard)** | **GPIO17** | **TX2** | **220–330 Ω** | UART-TX vom ESP32 zum PowerBoard-RX. Sehr empfohlen für Signalintegrität & Schutz. |
| **UART_RX (PowerBoard → Client)** | **GPIO16** | **RX2** | **0–220 Ω (optional)** | UART-RX am ESP32. Standard 0 Ω; optional kleiner Serienwiderstand bei langen/noisy Leitungen. |

**Layout-Hinweis:**  
Serienwiderstände sollten **nah am ESP32-GPIO** platziert werden (Quellseite).

---

## 2) Signale, die *nicht* pauschal mit 220–330 Ω seriell ausgeführt werden

### 2.1 Door-Signal (Eingang)
| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| **OVEN_DOOR_SENSOR** | **GPIO14** | **PIN_CH5** | **kein 220–330 Ω Standard** | Eingangssignal. Üblich sind Pull-up/Pull-down und ggf. RC-Filter. Serien-R eher 1–4,7 kΩ falls benötigt. |

**Typische Eingangsbeschaltung (Beispiele):**
- Pull-up: 10 kΩ nach 3,3 V (oder interner Pull-up)
- Optional RC-Filter: 1–4,7 kΩ + 10–100 nF
- Optional ESD-Schutz bei langen Leitungen

---

### 2.2 Analoge Eingänge (ADC)
| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| *(ADC)* | GPIO36 | PIN_ADC0 | kein 220–330 Ω | Analogeingang. RC-Filter & Schutz gezielt auslegen. |
| *(ADC)* | GPIO39 | PIN_ADC1 | kein 220–330 Ω | Analogeingang. RC-Filter & Schutz gezielt auslegen. |
| *(ADC)* | GPIO34 | PIN_ADC2 | kein 220–330 Ω | Analogeingang. RC-Filter & Schutz gezielt auslegen. |
| *(ADC)* | GPIO35 | PIN_ADC3 | kein 220–330 Ω | Analogeingang. RC-Filter & Schutz gezielt auslegen. |

**Begründung:**  
Ein pauschaler Serienwiderstand kann die ADC-Messung verfälschen (Quellimpedanz, Sample-and-Hold).

---

### 2.3 SPI (MAX6675, optional)
| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| *(MAX6675_SCK)* | GPIO18 | PIN_MAX6675_SCK | 220–330 Ω (optional) | SPI-Takt, schnelle Flanken → optional zur Dämpfung bei längeren Leitungen. |
| *(MAX6675_CS)* | GPIO5 | PIN_MAX6675_CS | 220–330 Ω (optional) | Optional bei längeren Leitungen. |
| *(MAX6675_SO / MISO)* | GPIO19 | PIN_MAX6675_SO | 0–220 Ω (optional) | Eingang am ESP32, meist 0 Ω ausreichend. |

---

## 3) Zusammenfassung – Was konkret bestücken?

### 3.1 Pflicht / Stark empfohlen
- **Alle Aktor-Steuerleitungen (MCU → PowerBoard)** mit **220–330 Ω**:  
  GPIO32, GPIO33, GPIO25, GPIO26, GPIO27, GPIO12
- **UART TX (GPIO17)** mit **220–330 Ω**

### 3.2 Optional / situationsabhängig
- **UART RX (GPIO16)**: 0 Ω Standard, optional 100–220 Ω
- **SPI SCK/CS**: optional 220–330 Ω bei längeren Leitungen

### 3.3 Nicht wie Ausgänge behandeln
- **Door Input (GPIO14)**: Pull-up + optional RC-Filter
- **ADC Inputs (GPIO36/39/34/35)**: gezielte Analogbeschaltung

---

## 4) Begründung – Warum Serienwiderstände?

### 4.1 Schutz der GPIOs
Serienwiderstände begrenzen den Strom bei:
- Kurzschluss gegen GND oder 3,3 V
- Fehlverdrahtung / Steckfehler
- Treiber-Kollisionen (MCU treibt gegen externen Treiber)

### 4.2 Bessere Signalqualität
GPIOs haben sehr schnelle Flanken. Leitungen wirken als Übertragungsleitungen:
- Überschwingen (Ringing)
- Fehltrigger an Treiber- oder Gate-Eingängen

Der Serienwiderstand dämpft diese Effekte zuverlässig.

### 4.3 EMV-Verbesserung
Gedämpfte Flanken → weniger hochfrequente Störenergie:
- geringere Abstrahlung
- weniger Einkopplung in Nachbarleitungen (z. B. UART)

### 4.4 Robustere Treiberansteuerung
Optokoppler, Transistorbasen und Gate-Netze verursachen Stromspitzen.
Der Serienwiderstand reduziert diese und erhöht die Robustheit.

### 4.5 Stabilere UART-Kommunikation
UART ist empfindlich auf Störungen rund um Start-/Stop-Bits.
Ein Serienwiderstand am **TX** ist eine einfache und sehr effektive Maßnahme.

---

## 5) Referenz: GPIO-Zuordnung (aus Projekt)

- OVEN_FAN12V = PIN_CH0 = GPIO32  
- OVEN_FAN230V = PIN_CH1 = GPIO33  
- OVEN_FAN230V_SLOW = PIN_CH2 = GPIO25  
- OVEN_LAMP = PIN_CH3 = GPIO26  
- OVEN_SILICAT_MOTOR = PIN_CH4 = GPIO27  
- OVEN_DOOR_SENSOR = PIN_CH5 = GPIO14  
- OVEN_HEATER = PIN_CH6 = GPIO12  

- UART: CLIENT_RX2 = GPIO16, CLIENT_TX2 = GPIO17
