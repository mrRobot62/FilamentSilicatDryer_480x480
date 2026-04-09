# ESP32-WROOM ↔ PowerBoard – GPIO Series Resistors (220–330 Ω)

This document summarizes which ESP32-WROOM GPIO signals should be connected to the PowerBoard **with series resistors**
(typically **220–330 Ω**), and why this is recommended.

The mapping is based on the GPIO list you provided (ESP32-WROOM "Client" side).

---

## 1) Recommended Series Resistors (Main Control & UART)

| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| **OVEN_FAN12V** | **GPIO32** | **PIN_CH0** | **220–330 Ω** | Digital output to PowerBoard driver channel (CH0). Series resistor recommended for protection and EMI reduction. |
| **OVEN_FAN230V** | **GPIO33** | **PIN_CH1** | **220–330 Ω** | Digital output (CH1) to TRIAC/driver path. Series resistor recommended. |
| **OVEN_FAN230V_SLOW** | **GPIO25** | **PIN_CH2** | **220–330 Ω** | Digital output (CH2) to TRIAC/driver path. Series resistor recommended. |
| **OVEN_LAMP** | **GPIO26** | **PIN_CH3** | **220–330 Ω** | Digital output (CH3) to driver path. Series resistor recommended. |
| **OVEN_SILICAT_MOTOR** | **GPIO27** | **PIN_CH4** | **220–330 Ω** | Digital output (CH4) to motor TRIAC/driver path. Series resistor recommended. |
| **OVEN_HEATER** | **GPIO12** | **PIN_CH6** | **220–330 Ω** | Digital output (CH6) to relay driver path (heater relay). Series resistor recommended. |
| **UART_TX (Client → PowerBoard)** | **GPIO17** | **TX2** | **220–330 Ω** | UART TX from ESP32 to PowerBoard RX. Strongly recommended to improve signal integrity and protect against wiring faults. |
| **UART_RX (PowerBoard → Client)** | **GPIO16** | **RX2** | **0–220 Ω (optional)** | UART RX at ESP32. Usually **0 Ω** is fine; **100–220 Ω** can help on long/noisy harnesses (optional). |

### Practical placement note
- Place the **series resistor close to the ESP32 GPIO pin** (driver side).  
  This improves protection and reduces ringing by damping the fast edge right at the source.

---

## 2) Signals that are *not* typically implemented with 220–330 Ω series resistors

These pins are present in your list, but they are usually treated differently.

### 2.1 Door input (OVEN_DOOR_SENSOR)
| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| **OVEN_DOOR_SENSOR** | **GPIO14** | **PIN_CH5** | **no 220–330 Ω series (default)** | Door sensor is an **input**. Typical measures are **pull-up/pull-down**, optional **RC filtering**, and optionally a **small series resistor** (e.g. 1–4.7 kΩ) if you explicitly want ESD/current limiting on the input. |

**Typical input conditioning (examples):**
- Pull-up: 10 kΩ to 3.3 V (or internal pull-up if reliable)
- Optional RC filter: 1–4.7 kΩ series + 10–100 nF to GND (debounce/noise)
- ESD protection diode/clamp if the harness is long and exposed

> Note: A 220–330 Ω series resistor on an input is *not wrong*, but it is not the “standard default” measure like it is for **outputs** and **UART TX**. For noisy door lines, an RC filter is usually the more effective tool.

### 2.2 Analog inputs (ADC)
| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| *(analog)* | GPIO36 | PIN_ADC0 | no 220–330 Ω series (default) | ADC input. Prefer RC filter and proper source impedance management. |
| *(analog)* | GPIO39 | PIN_ADC1 | no 220–330 Ω series (default) | ADC input. Prefer RC filter and proper source impedance management. |
| *(analog)* | GPIO34 | PIN_ADC2 | no 220–330 Ω series (default) | ADC input. Prefer RC filter and proper source impedance management. |
| *(analog)* | GPIO35 | PIN_ADC3 | no 220–330 Ω series (default) | ADC input. Prefer RC filter and proper source impedance management. |

**Why not “blindly” 220–330 Ω on ADC?**
- ADC readings can be affected by the source impedance and the sampling capacitor behavior.
- If you need protection/noise filtering, use a **defined RC filter** and confirm it against sampling requirements.

### 2.3 SPI (MAX6675) – optional depending on wiring length
You listed MAX6675 pins as an example (SCK=GPIO18, CS=GPIO5, SO/MISO=GPIO19).

| Oven | GPIO | Channel | Widerstand | Info |
|---|---|---|---:|---|
| *(MAX6675_SCK)* | GPIO18 | PIN_MAX6675_SCK | 220–330 Ω (optional) | SPI clock has fast edges; series resistor can reduce EMI/ringing on longer wires. |
| *(MAX6675_CS)* | GPIO5 | PIN_MAX6675_CS | 220–330 Ω (optional) | Optional for long harnesses; improves edge damping and protection. |
| *(MAX6675_SO / MISO)* | GPIO19 | PIN_MAX6675_SO | 0–220 Ω (optional) | Input to ESP32. Usually 0 Ω is fine; small resistor can help if ringing occurs. |

---

## 3) Summary (What to implement)

### 3.1 “Must / Strongly Recommended”
- **All PowerBoard control outputs**:  
  **GPIO32, GPIO33, GPIO25, GPIO26, GPIO27, GPIO12 → 220–330 Ω series**
- **UART TX to PowerBoard**:  
  **GPIO17 (TX2) → 220–330 Ω series**

### 3.2 “Optional / Situation-dependent”
- **UART RX from PowerBoard**:  
  **GPIO16 (RX2) → 0 Ω default, 100–220 Ω optional**
- **SPI SCK/CS** (if MAX6675 is wired via harness / longer distance):  
  **GPIO18, GPIO5 → 220–330 Ω optional**
- **SPI MISO**:  
  **GPIO19 → 0 Ω default, small series optional**

### 3.3 “Do not treat like outputs”
- **Door sensor input (GPIO14)**: prefer pull-up + optional RC filter
- **ADC inputs (GPIO36/39/34/35)**: prefer RC filter/protection designed for ADC behavior

---

## 4) Why series resistors are necessary / recommended

Series resistors (220–330 Ω) between MCU GPIOs and an external board/harness are a proven best practice for robustness.

### 4.1 GPIO protection (short circuits, wiring mistakes, contention)
In real builds, failures happen:
- A pin can be accidentally shorted to GND or 3.3 V,
- a connector can be misaligned,
- an external stage may momentarily drive a line when the MCU also drives it.

A series resistor limits the instantaneous current so the GPIO and/or the external stage is less likely to be damaged.

### 4.2 Signal integrity (damping fast edges, reducing ringing/overshoot)
GPIO outputs have fast edges; a cable/harness behaves like a transmission line:
- inductance and capacitance cause **ringing** and **overshoot**,
- this can lead to false switching (especially on sensitive driver inputs),
- it increases EMI emissions.

A 220–330 Ω resistor plus input capacitance creates a controlled edge and damps ringing.

### 4.3 EMI reduction (practical EMC improvement)
Lower edge rate = lower high-frequency energy:
- fewer radiated emissions,
- fewer conducted disturbances on the harness,
- less chance to upset adjacent lines (e.g., UART lines near actuator controls).

### 4.4 External driver input behavior (optos/transistors/triac gates)
PowerBoard inputs often go into:
- optocoupler LEDs,
- transistor bases,
- gate networks for MOSFETs / TRIAC gate drive.

These structures can draw current spikes during transitions. The series resistor reduces peaks and makes the system more tolerant.

### 4.5 UART stability (especially TX)
UART decoding is sensitive around thresholds on the start bit:
- ringing/overshoot can create extra edges,
- long harnesses and ground bounce make this worse.

A series resistor at **TX** is one of the simplest ways to increase UART robustness.

---

## 5) Implementation hints (practical)

- Put the resistor **close to the ESP32** (source side).
- Use one resistor **per line** (do not share).
- Prefer 220 Ω if you need slightly faster edges; prefer 330 Ω if wiring is long/noisy.
- If a line is particularly noisy, consider:
  - twisted pair with GND reference,
  - adding a defined RC filter (inputs),
  - improving grounding and return paths.

---

## 6) Reference: Provided GPIO mapping (for traceability)

- OVEN_FAN12V = PIN_CH0 = GPIO32  
- OVEN_FAN230V = PIN_CH1 = GPIO33  
- OVEN_FAN230V_SLOW = PIN_CH2 = GPIO25  
- OVEN_LAMP = PIN_CH3 = GPIO26  
- OVEN_SILICAT_MOTOR = PIN_CH4 = GPIO27  
- OVEN_DOOR_SENSOR = PIN_CH5 = GPIO14  
- OVEN_HEATER = PIN_CH6 = GPIO12  

- Serial2: CLIENT_RX2 = GPIO16, CLIENT_TX2 = GPIO17
