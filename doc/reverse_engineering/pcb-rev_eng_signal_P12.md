## Signalverlauf P12 – Türschalter (DOOR)

### 1) Eingang (DOOR-Stecker, 2-polig)
- **Pin 1: GND**
- **Pin 2: +5 V** über internen Pull-Up-/Teilerwiderstand → **Schalter** → GND  
  → **Tür geschlossen** = Schalter **zu** ⇒ DOOR-Signal **LOW (≈0 V)**  
  → **Tür offen** = Schalter **auf** ⇒ DOOR-Signal **HIGH (≈5 V)**

> Direkt hinter dem DOOR-Stecker sitzt üblicherweise ein **RC-Filter** (Entprellung).

### 2) Verteilung / Fan-Out des DOOR-Signals
Vom gefilterten DOOR-Knoten gehen **drei Abzweige** ab:

A) **P12 (an den uC)**  
- DOOR → (Serien-R) → **P12** auf dem 12-poligen Verbinder.  
- Der uC liest den Zustand (LOW=zu, HIGH=offen) für Anzeige/Logik.

B) **Hardware-Sperre Heizung (Relais-Zweig)**  
- DOOR → **Sperr-NPN** (Basis über R).  
- **Emitter** an GND, **Kollektor** greift in die **Relais-Treiberbasis** (Q7-Umfeld) ein.  
- **Tür offen (HIGH)** ⇒ Sperr-NPN zieht Treiberbasis **nieder** ⇒ **Relais AUS** (Heizung sicher aus).

C) **Hardware-Sperre Hochvolt-Lasten (Optokoppler-Zweig)**  
- DOOR → Widerstandsleiste (die „470“-Reihe) → **Vorstufen** der Optokoppler **PD1–PD4**.  
- **Tür offen (HIGH)** ⇒ DOOR entzieht/kurzschließt den LED-Strom der Optokoppler ⇒ **Triacs Q1–Q4 AUS**  
  (betroffen: **FAN 230 V, LAMP 230 V, MOTOR 230 V, FAN-L 230 V**).

*(Optional, je nach Bestückung)*  
D) **Beeper-Pfad**  
- DOOR → kleiner R → Beeper-Treiber (Q6) → akustische Warnung bei offener Tür.

### 3) Zustandsmatrix

| Türzustand | DOOR-Knoten | **P12 (zum uC)** | Heizung (Relais) | PD1–PD4 / Triacs | Beeper |
|---|---|---|---|---|---|
| **geschlossen** | LOW (≈0 V) | LOW | **freigegeben** (uC darf schalten) | **freigegeben** (uC darf schalten) | aus |
| **offen** | HIGH (≈5 V) | HIGH | **hart gesperrt** | **hart gesperrt** | optional **an** |

### 4) Kernaussage
- **P12** ist **die an den uC weitergereichte DOOR-Logik** (LOW=zu, HIGH=offen).  
- **Gleichzeitig** wird das DOOR-Signal **hardwareseitig** auf dem Powerboard zur **Sicherheitsverriegelung** genutzt:  
  Heizung **aus**, alle 230-V-Lasten **aus**, optional **Beeper an** – unabhängig von der uC-Software.

### 5) Verifikation (nur Sekundärseite messen!)
- Bezug: **P3 (GND)**  
- **Tür offen:** DOOR-Pad ≈ **+5 V**, **P12 ≈ +5 V**, **P6/P7–P10 ≈ 0 V**  
- **Tür zu:** DOOR-Pad ≈ **0 V**, **P12 ≈ 0 V**; uC kann nun **P6/P7–P10** aktiv **auf ≈5 V** ziehen.