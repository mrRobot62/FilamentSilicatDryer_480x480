# Erweiterte Analyse des Powerboards (Hochvolt-Sektion)
**System:** SC92F8463BM-basiertes Steuer- und Leistungsboard  
**Datum:** 31. Oktober 2025  

---

## Powerboard – Anschluss P1–P12

|   Pin   | Bezeichnung | Typ / Richtung                 | Signalpfad (vereinfacht)                              | Beschreibung / Funktion                                                                                                                                                     |
| :-----: | :---------- | :----------------------------- | :---------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **P1**  | TEMP        | Eingang (analog)               | → Spannungsteiler → ADC des MCU-Boards                | Anschluss des NTC-Temperatursensors. Spannung sinkt bei steigender Temperatur.<br>P1-R12-NTC-5V                                                                             |
| **P2**  | +5 V        | Ausgang (DC)                   | ← 7805 / Sekundärnetzteil                             | Haupt-Versorgungsspannung für Steuerboard, Sensoren und Logik.                                                                                                              |
| **P3**  | GND         | Bezugspotential                | ← Sekundär-GND                                        | Masse aller Logik- und Sensorkreise (galvanisch getrennt von Netz).                                                                                                         |
| **P4**  | NC          | –                              | –                                                     | Nicht belegt (möglicherweise Reserveleitung).                                                                                                                               |
| **P5**  | FAN12V      | Eingang (digital), Ausgang     | → Transistor Q10 → Lüfter 12V                         | Vermutung: wird geprüft ob der Kühllüfter läuft. Lüfter liegt an Basis von Q10<br>R16-Kollector(Q10)-Basis(10)-12VLüfter-Emitter(GND), Test bei 5V auf P5 springt Lüfter an |
| **P6**  | HEATER REL  | Ausgang (PWM / Toggle)        | → RC/Filter → Transistor Q7 → Relais-Spule            | **Wichtig (Messung T9):** Kein DC-Level. **Heater-Enable benötigt PWM/Toggle** (aktuell gemessen: **4 kHz**, **50% Duty**, Periode ~252 µs). PWM läuft solange geheizt werden soll; bei STOP/Fehler sofort aus.
| **P7**  | FAN 230V    | Ausgang (digital, Optokoppler) | → Q8 → PD1 → Q1 (Triac) → 230 V-Lüfter                | Fan & fan-l gehen an den Spaltmotor und schalten den Lüfter inkl. rotes Kabel. Test OK, P7=5V=Fan FAST                                                                      |
| **P8**  | LAMP 230V   | Ausgang (digital, Optokoppler) | → Q9 → PD2 → Q2 (Triac) → 230 V-Lampe                 | Schaltet Innenbeleuchtung oder Heizraumlampe.                                                                                                                               |
| **P9**  | MOTOR 230V  | Ausgang (digital, Optokoppler) | → Q10 → PD3 → Q3 (Triac) → 230 V-Motor                | Schaltet den Antriebsmotor Drehspieß                                                                                                                                        |
| **P10** | FAN-L 230V  | Ausgang (digital, Optokoppler) | → Q11 → PD4 → Q4 (Triac) → 230 V-Sekundärlüfter       | FAN & FAN-L Lüfter, schwarzes Kabel. Test OK, P10=5V=FAN slow                                                                                                               |
| **P11** | NC          | –                              | –                                                     | Nicht belegt (Liegt aber 5V permanent an über Logic-Board)                                                                                                                  |
| **P12** | DOOR        | Eingang (digital)              | → Türschalter / Pull-Down (CLOSED=GND)               | **Messung T9:** **CLOSED=GND**, **OPEN=+5 V** ⇒ Logik: **Door OPEN = HIGH**, **Door CLOSED = LOW**. Wird als Safety-Input genutzt (z. B. Motor nur bei Door CLOSED).
|         |             |                                |                                                       |


### ESP32‑WROOM ↔ Powerboard‑Stecker (P1–P12) Mapping (Client)

> Basis: `pins_client.h` (Client) + aktuelle Messungen (T9).

| Stecker‑Pin | Powerboard‑Signal | Richtung (Powerboard) | ESP32‑WROOM GPIO | Client‑Define | Hinweis |
|---|---|---|---:|---|---|
| **P1** | TEMP (NTC) | Input (analog) | **GPIO36** | `OVEN_TEMP1_PORT1` / `PIN_ADC0` | Raw ADC, NTC über Teiler |
| **P2** | +5V | Output (DC) | – | – | Versorgung vom Powerboard |
| **P3** | GND | Reference | – | – | Logik‑GND |
| **P4** | NC | – | – | – | – |
| **P5** | FAN12V | Output (12V) | **GPIO32** | `OVEN_FAN12V` / `PIN_CH0` | Kühl‑Lüfter Powerboard |
| **P6** | HEATER REL | Output (Enable) | **GPIO12** | `OVEN_HEATER` / `PIN_CH6` | **PWM 4 kHz / 50%** erforderlich |
| **P7** | FAN 230V | Output | **GPIO33** | `OVEN_FAN230V` / `PIN_CH1` | “FAST” |
| **P8** | LAMP 230V | Output | **GPIO25** | `OVEN_LAMP` / `PIN_CH2` | Door‑unabhängig |
| **P9** | MOTOR 230V | Output | **GPIO26** | `OVEN_SILICAT_MOTOR` / `PIN_CH3` | **nur bei Door CLOSED** |
| **P10** | FAN‑L 230V | Output | **GPIO27** | `OVEN_FAN230V_SLOW` / `PIN_CH4` | “SLOW” |
| **P11** | NC | – | – | – | – |
| **P12** | DOOR | Input (digital) | **GPIO14** | `OVEN_DOOR_SENSOR` / `PIN_CH5` | **OPEN=HIGH, CLOSED=LOW** |


### Aktueller Stand (T9) – verifizierte Logik (Messungen)

- **DOOR (P12):** `CLOSED = GND (LOW)`, `OPEN = +5 V (HIGH)` ⇒ *Door OPEN ist HIGH*.
- **HEATER (P6):** *Enable nur über PWM/Toggle*, gemessen **4 kHz**, **50% Duty**, Periode ~**252 µs**. **DC HIGH/LOW reicht nicht.**
- **MOTOR (P9):** darf **nur** laufen, wenn **Door CLOSED** (Safety-Gating im Client).
- **LAMP (P8):** Door‑unabhängig (kein Safety‑Gating erforderlich).
- **FAN12V (P5):** läuft im aktuellen Setup als **Kühlung des Powerboards** (Policy/Implementation kann bewusst unabhängig von Heater bleiben).




## Test 2025-11-04
- P1 NTC, im Test keinerlei Veränderung der Spannung, muss nochmals geprüft werden
- P2 5V
- P3 GND
- P4 NC
- P5 Fan12 V, wenn 5V anliegt, dreht der 12V Lüfter. Der Lüfter muss aber drehen, wenn die Heizung angeht, wegen kühlung des powerBoards - wichtig
- P6 Heater, ist trotz 5V nicht angesprungen. Ggf wegen fehlendem Fan12=On, Fan230=On, Tür war aber geschlossen. Erneuter Test mit Steuerbord
- P7 Funktioniert, Fan dreht schnell (und hörbar lauter)
- P8 LAMP, funktioniert bei 5V
- P9 bei 5V, Drehspießmotor dreht
- P10 5V FAN-L (L=Low) motor dreht langsamer
- P11 NC
- P12 muss nochmals geprüft werden, wenn Steuerbord angeschlossen ist                                                               |

## 🧩 Ergänzende Hinweise
- Messreferenz:
  - Immer gegen P3 (GND) messen.
  - Alle Pins P1–P12 sind galvanisch getrennt von der Netzspannung.       
- Signalrichtung:
    - „Ausgang (digital)“ → vom Powerboard (bzw. MCU auf dem Steuerboard) gesteuert.
    - „Eingang“ → liefert Status oder Sensorwert ans
- Treiberstruktur für 230 V-Lasten (P7–P10):
  - MCU → Transistor Q8..Q11 → Optokoppler PD1..PD4 → Triac Q1..Q4 → 230 V-Last
- Heizungsrelais (P6):
  - Wird über separaten 5 V-Treiber (Q7) geschaltet.
- Relais trennt 230 V-Heizkreis, wenn Solltemperatur erreicht ist.
  - Door-Signal (P12):

Meist als „Safety Interlock“ implementiert – blockiert Heizung und Motor, wenn offen.



## Mess-Leitfaden (P1–P12) – erwartete Spannungen & Prüfhinweise

> **Bezug (GND):** immer **P3**.  
> **Messgerät:** DC-Volt. Nur Sekundärseite messen (galvanisch getrennt).  
> **Hinweis:** „aktiv“ = Funktion eingeschaltet (per Gerätelogik/Bedienung).

|   Pin   | Signal             | Leerlauf (typ.) |       Aktiv (typ.)        | Wie auslösen / prüfen                          | Erwartetes Verhalten / Hinweis                              |
| :-----: | :----------------- | :-------------: | :-----------------------: | :--------------------------------------------- | :---------------------------------------------------------- |
| **P1**  | TempSensor (NTC)   |    0.5–4.5 V    |        ändert sich        | Fühler anfassen/erwärmen oder abkühlen         | Spannung **sinkt** bei Erwärmung (NTC). Ruhig, ohne Ripple. |
| **P2**  | +5 V VCC           |    **5.0 V**    |         **5.0 V**         | Gerät eingeschaltet                            | Stabile 5 V (≤±5 %). Deutliches Absacken ⇒ Netzteil prüfen. |
| **P3**  | GND                |       0 V       |            0 V            | –                                              | Bezugspunkt für alle Messungen.                             |
| **P4**  | NC                 |       0 V       |            0 V            | –                                              | Nicht belegt.                                               |
| **P5**  | Fan 5 V            |       0 V       | 5 V (oder PWM-Mittelwert) | Lüfterfunktion starten                         | Bei PWM zeigt DMM 2–4 V Mittelwert, Oszi sähe Rechteck.     |
| **P6**  | HEATER REL  | Ausgang (PWM / Toggle)        | → RC/Filter → Transistor Q7 → Relais-Spule            | **Wichtig (Messung T9):** Kein DC-Level. **Heater-Enable benötigt PWM/Toggle** (aktuell gemessen: **4 kHz**, **50% Duty**, Periode ~252 µs). PWM läuft solange geheizt werden soll; bei STOP/Fehler sofort aus.
| **P7**  | Fan 230 V (Opto)   |      ~0 V       |          **5 V**          | 230 V-Lüfter einschalten                       | HIGH treibt Optokoppler → Triac zündet (Netzseite).         |
| **P8**  | Lamp 230 V (Opto)  |      ~0 V       |          **5 V**          | Lampe einschalten                              | Wie P7.                                                     |
| **P9**  | Motor 230 V (Opto) |      ~0 V       |          **5 V**          | Motorfunktion starten                          | Wie P7.                                                     |
| **P10** | FAN-L 230 V (Opto) |      ~0 V       |          **5 V**          | 2. Lüfterleitung/FAN-L aktivieren              | Häufig gekoppelt mit P5 oder Heizung (logisch verknüpft).   |
| **P11** | NC                 |       0 V       |            0 V            | –                                              | Nicht belegt.                                               |
| **P12** | DOOR        | Eingang (digital)              | → Türschalter / Pull-Down (CLOSED=GND)               | **Messung T9:** **CLOSED=GND**, **OPEN=+5 V** ⇒ Logik: **Door OPEN = HIGH**, **Door CLOSED = LOW**. Wird als Safety-Input genutzt (z. B. Motor nur bei Door CLOSED).

###

### Zusatz-Messpunkte (Sekundärseite)
| Punkt                        | Leerlauf |     Aktiv     | Hinweis                                    |
| :--------------------------- | :------: | :-----------: | :----------------------------------------- |
| **Relais-Spule** (gegen GND) |   0 V    |     ~5 V      | Nur wenn Heizung EIN (P6 = PWM 4 kHz / 50%).            |
| **Beeper** (gegen GND)       |   0 V    | pulsend 0–5 V | Je nach Tonfrequenz.                       |
| **Ausgang 7805** (Pin OUT)   |  5.0 V   |     5.0 V     | Falls deutlich <4.8 V: Last/Regler prüfen. |

### Sicherheit
- **Nur** auf der **Sekundärseite** messen (Bereich um 7805, Relais, Stecker **P1–P12**).  
- **Keine Messung** an Primärseite (Gleichrichter, X2-Kondensator, 400 V-Elko, PN8034M) ohne Trenntrafo!



## ⚡ Übersicht der 230-V-Treibersektion

Die Hochvolt-Stufe steuert vier getrennte 230-V-Verbraucher über Halbleiterschalter (Triacs oder MOSFETs, gekennzeichnet als **Q1–Q4**)  
sowie ein Relais für die Heizung.

### Komponentenübersicht
| Kennzeichnung | Funktion | Angesteuerter Verbraucher   | Typ               | Bemerkung                                |
| :------------ | :------- | :-------------------------- | :---------------- | :--------------------------------------- |
| **Q1**        | FAN      | Lüfter 230 V                | Triac / Optotriac | Schaltet Hauptlüfter                     |
| **Q2**        | LAMP     | Lampe 230 V                 | Triac / Optotriac | Beleuchtung                              |
| **Q3**        | MOTOR    | Motor 230 V                 | Triac / Optotriac | Antrieb (z. B. Rührwerk / Ventil)        |
| **Q4**        | FAN-L    | Lüfterleitung (Low/Neutral) | Triac / MOSFET    | Zweite Lüfterleitung / Phasenumschaltung |
| **RELAY**     | HEATER   | Heizlast 230 V              | Relais            | Spule 5 V, galvanisch getrennt           |

![Powerboard Gesamtansicht](img/PowerPlatine_PCB_SIDE.jpeg)  


---

## 🔌 Hochvolt-Anschlüsse
![Powerboard Gesamtansicht](img/PowerPCB_220V_Teil.jpeg)  

| Anschluss    | Beschreibung                                     |
| :----------- | :----------------------------------------------- |
| **HEATER**   | Ausgang zum Heizstab / Heizplatte                |
| **FAN**      | Hochvolt-Lüfter                                  |
| **LAMP**     | Beleuchtung 230 V                                |
| **MOTOR**    | Motor oder Antrieb 230 V                         |
| **FAN-L**    | Zweite Lüfterleitung oder Neutral-Pfad           |
| **230 V IN** | Netzspannungseingang (L/N) für alle Triac-Zweige |

Die Leiterbahnen zwischen „230 V IN“ und den vier Schaltzweigen sind breit ausgeführt,  
um höhere Ströme sicher zu führen. Zwischen 230-V- und 5-V-Bereich befindet sich  
ein klar sichtbarer **Isolationsschlitz**.

---

## ⚙️ Ansteuerlogik

Jeder der Transistor-Zweige **Q1–Q4** wird von einem Signal aus der 5-V-Logik (MCU-Board) angesteuert:  
- Signalleitungen kommen über Steckerpins **P7–P10** (siehe bisherige Analyse).  
- Jede Leitung treibt einen Optokoppler oder eine Gate-Vorstufe (T1-G-T2-Beschriftung deutet auf Triacs hin).  
- Diese wiederum schalten den jeweiligen 230-V-Lastzweig.  
- Dadurch besteht **galvanische Trennung** zwischen Mikrocontroller und Netzspannung.

Die **Heizungssteuerung** erfolgt **nicht über Triac**, sondern über ein separates **Relais**,  
das auf der Platine direkt sichtbar ist. Dieses Relais ist mit der 5-V-Spule über einen Transistor  
angesteuert (Low-Side-Schaltung mit Freilaufdiode).

---

## 🔒 Sicherheit und Isolation

- Deutlich sichtbarer **Isolationsschlitz** zwischen Hochvolt- und Niedervolt-Bereich.  
- Jede Steuerleitung (P7–P10) geht über einen Optokoppler (PD1–PD4) zu den Triacs Q1–Q4.  
- Alle Netzleitungen sind großflächig verzinnt, mit dicken Leiterbahnen und ausreichenden Kriechstrecken.  
- Die 5-V-Versorgung (MCU-Seite) bleibt vollständig getrennt von der Netzseite.

---

## 🔍 Zusammenfassung der Funktionsgruppen

| Bereich                              | Spannungsebene | Steuerung                  | Verbraucher                 |
| :----------------------------------- | :------------- | :------------------------- | :-------------------------- |
| **Heater (Relais)**                  | 230 V          | 5 V-Relais über Transistor | Heizplatte / Heizelement    |
| **Fan 230 V (Q1)**                   | 230 V          | Optokoppler + Triac        | Hauptlüfter                 |
| **Lamp 230 V (Q2)**                  | 230 V          | Optokoppler + Triac        | Beleuchtung                 |
| **Motor 230 V (Q3)**                 | 230 V          | Optokoppler + Triac        | Antrieb                     |
| **Fan-L (Q4)**                       | 230 V          | Optokoppler + Triac        | Zweite Lüfterleitung        |
| **Beeper / Fan 5 V / Sensor / Door** | 5 V            | Transistor + MCU-Ports     | Logik- und Signalfunktionen |

---

## 🧠 Gesamtfunktion

1. Das Powerboard erhält eine Eingangsspannung von **230 V AC**.  
2. Ein internes Netzteil oder externer Adapter liefert **12 V–24 V DC**,  
   die über den **7805-Regler** auf **5 V** stabilisiert werden.  
3. Diese 5 V speisen die MCU-Logik, Sensorik und Niedervolt-Treiber.  
4. Steuerbefehle des Mikrocontrollers (über P1–P12) aktivieren die  
   jeweiligen Transistor- oder Relaisstufen.  
5. Optokoppler sorgen für elektrische Isolation zur Hochvolt-Schaltseite.  
6. Q1–Q4 schalten unabhängig die 230-V-Lasten (Lüfter, Lampe, Motor, Lüfter-Neutral).  
7. Das Relais schaltet die Heizleistung.

---

## ✅ Fazit

Das Powerboard ist ein **komplettes, galvanisch getrenntes Leistungsmodul**  
zur Ansteuerung mehrerer 230-V-Verbraucher.  
Es kombiniert **eine lineare 5-V-Versorgung**, **Transistor-Treiberstufen**, **Optokoppler**,  
**Triac-Schalter** und **ein Relais** zu einem sicheren Interface zwischen  
Netzspannung und Mikrocontrollerlogik.

---



# Analyse der Transistorsektion Q5–Q11  
**System:** SC92F8463BM-basiertes Steuer-/Powerboard  
**Datum:** 31. Oktober 2025  

---

## 🧩 Überblick

Die Transistoren **Q5–Q11** befinden sich auf der Niedervolt-Seite (5 V-Logikbereich)  
und bilden die vollständige Treiber- und Kopplungslogik zwischen dem Mikrocontroller  
und den angeschlossenen Verbrauchern (Relais, Lüfter, Optokoppler-Treiber).

Sie übernehmen folgende Aufgaben:
- Direktes Schalten von 5 V-Lasten (Relais, Beeper, 5 V-Lüfter)  
- Logische Kopplung zwischen Signalen (z. B. Heizung → Lüfter)  
- Ansteuerung von Optokopplern (PD1–PD4) für galvanisch getrennte 230 V-Stufen  

---

## ⚙️ Einzelanalyse Q5–Q11

| Bauteil | Position / Bezug               | Funktion                                                | Beschreibung                                                                                                       |
| :-----: | :----------------------------- | :------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------- |
| **Q5**  | rechts neben Relais            | Hilfstransistor, erhält Signal vom Relais-Treiber (Q7)  | Dient als Zwischenglied, um Heizungsaktivität an weitere Schaltung (z. B. Lüfter oder Optokoppler) weiterzugeben   |
| **Q6**  | Nähe „Beeper“-Bereich          | Beeper-Treiber                                          | Schaltet den Piezo-Beeper gegen Masse (Low-Side, aktiv LOW)                                                        |
| **Q7**  | direkt am Relais               | Relais-Treiber (Heizung)                                | NPN-Transistor, schaltet Relaisspule, besitzt Freilaufdiode; wird direkt von MCU angesteuert                       |
| **Q8**  | rechts vom Relais, Basis an Q5 | Logik- oder Folgetreiber, evtl. Optokoppler-Ansteuerung | Wird nur aktiv, wenn Heizung (Q7) bzw. Q5 aktiv ist – koppelt Heizsignal weiter, z. B. für Lüfterstart bei Heizung |
| **Q9**  | Bereich Mitte/oben             | Möglicher Optokoppler-Treiber                           | Schaltet 230 V-Last (z. B. Lampe oder Motor) synchron zu einem 5 V-Signal                                          |
| **Q10** | rechts bei P3/P4               | Vom 5 V-Lüfterkreis gesteuert                           | Wird durch das Lüftersignal aktiviert; könnte als Verstärker- oder Kopplungsstufe zum 230 V-Lüfter dienen          |
| **Q11** | oberhalb von PD4               | Optokoppler-Treiber (Fan-L oder Motor)                  | Schließt die Hochvolt-Kette; steuert den letzten Optokoppler-Zweig (PD4 → Triac Q4)                                |

---

## 🔍 Beobachtete Signalverbindungen

- **Q10 → Basis an 5 V-Lüfter** → Schaltet vermutlich einen Hochvolt-Lüfter synchron.  
- **Q8 → Basis an Q5 → Basis an Relais (Q7)** → Heizungsaktivität löst sekundäre Schaltung aus  
  (z. B. Sicherheit oder automatischer Lüfterlauf).  
- **Q5–Q11** bilden somit teils **mehrstufige oder rückgekoppelte Transistorstufen**,  
  die logische UND/ODER-Funktionen zwischen Signalen realisieren.

---

## 🧠 Wahrscheinliches Schaltungskonzept

Das Board verwendet eine **signalabhängige Kopplungslogik**,  
bei der einzelne Ausgänge automatisch mitgeschaltet werden,  
ohne dass der Mikrocontroller jeden Pin einzeln ansteuert.

## 🔸 Beispiel 1 – Heizungslogik

Diese Logik beschreibt die Abhängigkeit zwischen **Heizung** und **Lüfter**.  
Ziel: Sobald die Heizung aktiv ist, soll automatisch auch der Lüfter (5 V oder 230 V) mit eingeschaltet werden.

### Schaltfolge
```text
MCU → Relais (Q7)
         │
         ▼
        Q5 ──► Q8 ──► Optokoppler (PDx) ──► Triac (Q1 oder Q4) ──► Lüfter 230 V
```

### Beschreibung

	1.	MCU aktiviert das Heizungsrelais über Q7.
	2.	Das Relaissignal steuert Q5 (Hilfstransistor).
	3.	Q5 aktiviert Q8, welcher einen Optokoppler ansteuert.
	4.	Der Optokoppler treibt einen Triac (z. B. Q1 = FAN 230 V).
	5.	Ergebnis: Heizung an → Lüfter an, ohne dass die MCU dafür einen separaten Pin benötigt.

### Vorteile
	•	Spart I/O-Pins am Mikrocontroller.
	•	Stellt sicher, dass beim Heizen immer eine Luftzirkulation aktiv ist.
	•	Erhöht Sicherheit und Kühlwirkung, auch bei Firmwarefehlern.

## 🔸 Beispiel 2 – Lüfterlogik

Diese Logik koppelt den **5 V-Lüfter** mit dem **230 V-Lüfterzweig** (Fan-L).

### Schaltfolge

```text
MCU → 5 V-Lüfter (P5)
         │
         ▼
        Q10 ──► Q11 ──► Optokoppler (PD4) ──► Triac (Q4) ──► FAN-L 230 V
```
### Beschreibung
1. **MCU** aktiviert den **5 V-Lüfterausgang (P5)**.  
2. Dieses Signal steuert die **Basis von Q10**.  
3. **Q10** treibt **Q11**, welcher den **Optokoppler PD4** aktiviert.  
4. Der Optokoppler zündet **Triac Q4** und schaltet damit den 230 V-Lüfterzweig.  
5. Ergebnis: Der **230 V-Lüfter** läuft immer dann, wenn der **5 V-Lüfter** aktiv ist.

### Vorteile
- Synchroner Betrieb beider Lüfterkreise (Low- und High-Voltage).  
- Logische Kopplung ohne Softwareaufwand.  
- Sichere galvanische Trennung zwischen 5 V-Logik und 230 V-Netz
---

## 🧩 Zusammenfassung

| Gruppe                                         | Transistoren | Steuerquelle                    | Zweck                                                  |
| :--------------------------------------------- | :----------- | :------------------------------ | :----------------------------------------------------- |
| **Direkte Niedervolt-Treiber**                 | Q6, Q7       | MCU-Signale                     | Beeper, Relais                                         |
| **Hilfs-/Kopplungsstufen**                     | Q5, Q8, Q10  | Signale von Relais / 5 V-Lüfter | Automatische Abhängigkeiten (z. B. Lüfter bei Heizung) |
| **Optokoppler-Treiber (Hochvolt-Ansteuerung)** | Q9, Q11      | Hilfsstufen (Q5/Q10)            | Ansteuerung der galvanisch getrennten 230 V-Triacs     |

---

## ✅ Fazit

Die Transistorgruppe **Q5–Q11** ist kein einfacher Satz individueller Treiber,  
sondern ein **kombiniertes logisches Steuernetz**, das interne Signale  
(Heizung, Lüfter, Beeper) miteinander verknüpft und daraus automatisch  
die Hochvolt-Schaltbefehle ableitet.

Dadurch kann der SC92F8463BM mit nur wenigen I/O-Pins  
alle Verbraucher (5 V und 230 V) effizient und sicher ansteuern.



# Analyse der Signalpegel und Schaltlogik – Connector P1–P12  
**System:** Powerboard mit SC92F8463BM  
**Datum:** 31. Oktober 2025  

---

## ⚙️ Grundversorgung

|  Pin   | Funktion   | Typ        | Beschreibung                                     |
| :----: | :--------- | :--------- | :----------------------------------------------- |
| **P2** | VCC (+5 V) | Versorgung | Ausgang des 7805-Reglers, versorgt MCU und Logik |
| **P3** | GND        | Versorgung | Gemeinsame Masse-Referenz für alle Signale       |

---

## 🌡️ Analoger Eingang

|  Pin   | Funktion               | Richtung         | Aktiv gegen | Beschreibung                                                                                                  |
| :----: | :--------------------- | :--------------- | :---------- | :------------------------------------------------------------------------------------------------------------ |
| **P1** | Temperatursensor (NTC) | Eingang (analog) | GND         | Spannungsteiler (NTC + Widerstand), Spannung sinkt bei steigender Temperatur. Wird durch MCU-ADC ausgewertet. |

---

## 🔥 Relais- und Heizungssteuerung

|  Pin   | Funktion     | Richtung | Aktiv gegen | Aktivpegel           | Beschreibung                                                                                                                                           |
| :----: | :----------- | :------- | :---------- | :------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **P6**  | HEATER REL  | Ausgang (PWM / Toggle)        | → RC/Filter → Transistor Q7 → Relais-Spule            | **Wichtig (Messung T9):** Kein DC-Level. **Heater-Enable benötigt PWM/Toggle** (aktuell gemessen: **4 kHz**, **50% Duty**, Periode ~252 µs). PWM läuft solange geheizt werden soll; bei STOP/Fehler sofort aus.

---

## 💨 Hochvolt-Ausgänge (Triac/Optokoppler-Stufen)

|   Pin   | Funktion    | Richtung | Aktiv gegen | Aktivpegel    | Beschreibung                                                       |
| :-----: | :---------- | :------- | :---------- | :------------ | :----------------------------------------------------------------- |
| **P7**  | Fan 230 V   | Ausgang  | GND         | **HIGH = an** | Schaltet Optokoppler PD1 → Triac Q1 → Hochvolt-Lüfter              |
| **P8**  | Lamp 230 V  | Ausgang  | GND         | **HIGH = an** | Schaltet Optokoppler PD2 → Triac Q2 → Beleuchtung                  |
| **P9**  | Motor 230 V | Ausgang  | GND         | **HIGH = an** | Schaltet Optokoppler PD3 → Triac Q3 → Motor / Antrieb              |
| **P10** | Fan-L 230 V | Ausgang  | GND         | **HIGH = an** | Gekoppelte Lüfterleitung, geschaltet über Q10/Q11 → PD4 → Triac Q4 |

> Die Hochvolt-Lasten (P7–P10) werden über **Optokoppler-LEDs mit Vorwiderständen** gesteuert.  
> Aktivierung erfolgt durch **positives 5-V-Signal vom MCU-Board**, das gegen GND referenziert ist.

---

## 🔊 Zusatzfunktionen

|     Pin      | Funktion | Richtung | Aktiv gegen | Aktivpegel       | Beschreibung                                                                              |
| :----------: | :------- | :------- | :---------- | :--------------- | :---------------------------------------------------------------------------------------- |
| **(intern)** | Beeper   | Ausgang  | GND         | **LOW = aktiv**  | Wird über Q6 geschaltet. Kein Steckerpin, aber gleiche Logik wie andere Low-Side-Treiber. |
| **(intern)** | Fan 5 V  | Ausgang  | GND         | **HIGH = aktiv** | Direkt von MCU gesteuert (P5-Leitung), treibt NPN-Transistor Q5.                          |

---

## 🚪 Türkontakt

|   Pin   | Funktion    | Richtung | Aktiv gegen | Aktivpegel                               | Beschreibung                                                                                                     |
| :-----: | :---------- | :------- | :---------- | :--------------------------------------- | :--------------------------------------------------------------------------------------------------------------- |
| **P12** | DOOR        | Eingang (digital)              | → Türschalter / Pull-Down (CLOSED=GND)               | **Messung T9:** **CLOSED=GND**, **OPEN=+5 V** ⇒ Logik: **Door OPEN = HIGH**, **Door CLOSED = LOW**. Wird als Safety-Input genutzt (z. B. Motor nur bei Door CLOSED).

---

## 🔁 Zusammenfassung der Signalpegel

| Kategorie                     | Pins                    | Pegel bei „aktiv“             | Referenz |
| :---------------------------- | :---------------------- | :---------------------------- | :------- |
| **Versorgung**                | P2, P3                  | +5 V / 0 V                    | –        |
| **Analoge Eingänge**          | P1                      | analog, gegen GND             | GND      |
| **Digitale Ausgänge (Logik)** | P5, P6, P7, P8, P9, P10 | **HIGH = EIN**                | GND      |
| **Digitale Eingänge**         | P12                     | **LOW = aktiv (geschlossen)** | GND      |

---

## 🧠 Logische Zusammenhänge

```text
Temperatursensor (P1)
   ↓
   └──► Heater Relay (P6, HIGH = EIN)
           │
           └──► Q5 → Q8 → PDx → Triac (Q1/Q4)
                     └──► Fan 230 V aktiv

Fan 5 V (P5)
   ↓
   └──► Q10 → Q11 → PD4 → Triac (Q4)
                     └──► Fan-L 230 V aktiv


Door Switch (P12)
   ↓
   └──► LOW = geschlossen → Betrieb erlaubt
        HIGH = offen → Heizung / Motor / Lüfter gesperrt

```

## ✅ Fazit
- Alle Signale sind gegen GND (P3) referenziert.
- Schaltleitungen (P5–P10, P6) sind active HIGH → 5 V-Signal schaltet Transistor- oder Optokoppler-Stufen ein.
- Eingänge (P1, P12) werden gegen GND ausgewertet.
- Die gesamte Logik arbeitet im 5-V-Bereich, galvanisch getrennt von der Netzseite.
- Der Aufbau folgt einem klassischen Muster: MCU-Board liefert logische HIGH-Signale, Powerboard schaltet Lasten gegen GND.



## Blockschaltbild (Mermaid)

```mermaid
flowchart LR
  A[230 V AC IN<br/>(L/N)] --> F[Sicherung 3.15A]
  F --> NTC[NTC / Einschaltstrombegrenzer]
  NTC --> BR[Brückengleichrichter<br/>(brauner 4-Pin-Block)]
  BR -->|+| C400[Elko ~400 V DC]
  BR -->|-| PGND[Primär-GND]

  C400 --> PN[PN8034M<br/>Off-line PWM Controller]
  PN --> TR[Übertrager / Trafo]

  %% Sekundär
  TR --> SD[Schottky-Diode]
  SD --> C5V[Elko 5–12 V DC]
  C5V --> VREG[5 V Rail<br/>(direkt oder via 7805)]
  C5V -.-> FB[TL431 + Optokoppler]
  FB -.-> PN

  %% Low-Voltage Seite zu Steuerboard
  VREG --> P2[(P2: +5 V)]
  PGND ---> P3[(P3: GND)]

  %% Sensoren/Eingänge
  P1[(P1: Temp NTC)] --> ADC[MCU-ADC auf Steuerboard]
  DOOR[(P12: Door)] --> MCUin[MCU-Eingang]

  %% 5V-Lasten
  VREG --> Q5[Q5: 5 V-Lüfter Treiber]
  Q5 --> FAN5V[Fan 5 V]

  VREG --> Q6[Q6: Beeper Treiber]
  Q6 --> BEEP[Beeper]

  %% Relais/Heizung
  VREG --> Q7[Q7: Relais-Treiber]
  Q7 --> RELAY[Heizungsrelais]
  RELAY --> HEAT[Heater 230 V]

  %% Gekoppelte Logik (Beispiele)
  Q7 -. Heizung aktiv .-> Q5
  Q5 -. optional Kopplung .-> Q8

  %% Optokoppler/Triacs Hochvolt
  VREG --> Q8[Q8..Q11: Optokoppler-Vorstufen]
  Q8 --> PD1[PD1..PD4: Optokoppler]
  PD1 --> T1[Q1..Q4: Triacs]

  %% Lasten 230V
  T1 --> FAN230[Fan 230 V]
  T1 --> LAMP[Lamp 230 V]
  T1 --> MOTOR[Motor 230 V]
  T1 --> FANL[FAN-L 230 V]

  %% Stecker-Pins zu Hochvolt-Ausgängen
  P7[(P7)] --> PD1
  P8[(P8)] --> PD1
  P9[(P9)] --> PD1
  P10[(P10)] --> PD1
  ```

## Blockschaltbild (ASCII-Fallback)
230 V AC IN (L/N)
           |
     [Sicherung F1]
           |
    [NTC / Inrush R]
           |
  [Brückengleichrichter]
        |         |
       (+)       (−)
        |         |
  [Elko ~400 V]  PGND  (Primär-GND)
        |
    [PN8034M]
        |
     [Trafo]
        |
 Sekundär-Gleichrichtung
        |
[Schottky] -> [Elko 5–12 V] -> [5 V Rail / 7805]
|                          |
(FB über TL431 + Optokoppler)
‘—————————’
           5 V LOGIKSEITE
```