# ESP32-S3 ↔ ESP32-WROOM Communication Protocol (Step 2 Documentation)

## Summary of Requirements

This document outlines the communication protocol between a HOST (ESP32-S3) and a CLIENT (ESP32-WROOM).  
The HOST provides a touchscreen user interface, while the CLIENT controls up to 16 I/O channels and reads various sensor inputs.  
Communication occurs via UART using a structured ASCII-based protocol.

### System Requirements

- **16 communication channels total**
  - **12 digital IN/OUT** channels (CH0–CH11)
  - **4 hybrid channels** (CH12–CH15) supporting:
    - Digital IN/OUT
    - Analog input (ADC raw values)
- **Temperature measurement** from a MAX6675 thermocouple module connected to the CLIENT.
- **UART-based ASCII protocol**, easy to debug and parse.
- **Hex-based 16‑bit digital mask** for all channel states.
- **Four ADC channels** transmitted as raw integer values (0–4095).
- **Temperature** transmitted as an integer in 0.25°C resolution: `temp_raw = °C × 4`.

---

# Step 2 – Communication Protocol Specification

## 1. Channel Definition

### 1.1 Channel Overview

| Channel | Bit | Mask     | Type               | Notes                        |
|---------|-----|-----------|--------------------|------------------------------|
| CH0     | 0   | 0x0001    | Digital IN/OUT     | Example: Heater              |
| CH1     | 1   | 0x0002    | Digital IN/OUT     | Example: Fan230V             |
| CH2     | 2   | 0x0004    | Digital IN/OUT     | Example: Fan230V-Slow        |
| CH3     | 3   | 0x0008    | Digital IN/OUT     | Example: Fan12V              |
| CH4     | 4   | 0x0010    | Digital IN/OUT     | Example: Lamp                |
| CH5     | 5   | 0x0020    | Digital IN/OUT     | Example: Motor               |
| CH6     | 6   | 0x0040    | Digital IN/OUT     | Free                         |
| CH7     | 7   | 0x0080    | Digital IN/OUT     | Free                         |
| CH8     | 8   | 0x0100    | Digital IN/OUT     | Free                         |
| CH9     | 9   | 0x0200    | Digital IN/OUT     | Free                         |
| CH10    | 10  | 0x0400    | Digital IN/OUT     | Free                         |
| CH11    | 11  | 0x0800    | Digital IN/OUT     | Free                         |
| CH12    | 12  | 0x1000    | Digital + Analog 0 | ADC channel A0               |
| CH13    | 13  | 0x2000    | Digital + Analog 1 | ADC channel A1               |
| CH14    | 14  | 0x4000    | Digital + Analog 2 | ADC channel A2               |
| CH15    | 15  | 0x8000    | Digital + Analog 3 | ADC channel A3               |

### 1.2 Digital Mask

A full 16‑bit digital mask represents the ON/OFF state of all channels.

- Format: **4‑digit HEX**, always padded, e.g.:
  - `0000` → all LOW  
  - `0001` → CH0 HIGH  
  - `1009` → CH0 + CH3 + CH12 HIGH

---

## 2. Analog Inputs

The CLIENT provides **four ADC values** mapped to channels:

| ADC Input | Channel | Field Name |
|-----------|----------|------------|
| ADC0      | CH12     | adc0_raw   |
| ADC1      | CH13     | adc1_raw   |
| ADC2      | CH14     | adc2_raw   |
| ADC3      | CH15     | adc3_raw   |

Values are raw integers **0–4095**.  
Voltage calculation and scaling are performed on the HOST.

---

## 3. Temperature Input (MAX6675)

- Temperature is transmitted as `temp_raw = °C × 4`
  - 25.00°C → `100`
  - 123.75°C → `495`

This preserves 0.25°C resolution.

---

## 4. UART Protocol Structure

Messages follow this structure:

```
<Sender>;<Command>[;Argument1;Argument2;...]\r\n
```

- **Sender:**  
  - `H` = HOST  
  - `C` = CLIENT  
- **Field separator:** `;`  
- **Line ending:** `
`

---

# 5. Commands

## 5.1 HOST → CLIENT Commands

### 5.1.1 SET – Write Digital Mask

Sets all 16 digital outputs.

```
H;SET;<bitmask16_hex>

```

Example:  
Enable CH0, CH3, CH4:

```
H;SET;0019

```

---

### 5.1.2 GET STATUS – Request Status Update

```
H;GET;STATUS

```

---

### 5.1.3 PING

```
H;PING

```

---

## 5.2 CLIENT → HOST Responses

### 5.2.1 ACK / ERR for SET

Success:

```
C;ACK;SET;<bitmask16_hex>

```

Error:

```
C;ERR;SET;<error_code>

```

---

### 5.2.2 STATUS – Full State Report

**Extended format supporting 16 digital channels + 4 ADCs + temperature:**

```
C;STATUS;<bitmask16_hex>;<adc0_raw>;<adc1_raw>;<adc2_raw>;<adc3_raw>;<temp_raw>

```

Example:

```
C;STATUS;0013;1000;2000;3000;4095;100

```

---

### 5.2.3 PONG

```
C;PONG

```

---

# End of Document
