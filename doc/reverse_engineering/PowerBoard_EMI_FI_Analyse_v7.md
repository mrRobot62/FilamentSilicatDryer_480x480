# PowerBoard 230 V – Vollanalyse & Zuordnung zur PN8034-Referenzschaltung (Update v7)

![🔗 Bild 1 – PowerBoard Topside (beschriftet)](PowerBoard_TopSide_230v_TopSide.png)  
![🔗 Bild 2 – PowerBoard Bottomside (230-V-Bereich)](PowerPCB_BottomSide_230V.png)  
![🔗 Bild 3 – PN8034 Referenzschaltung (Chipown)](PN8034_wiring.png)

---

## Update (gegenüber v6)

Neu berücksichtigt:

1) **J1 / 12‑pol Connector Pin 2 = 5V+** versorgt die externe MCU/Display-Elektronik.  
→ Das PowerBoard ist die **Source** für **Spannung und Strom** der angeschlossenen Steuerung.

2) Messwerte aus deiner Prüfung:
- Schutzleiterwiderstand (PE→Gehäuse): **0,06 Ω**
- Isolationswiderstand Heizelement (bei 500 V DC): **~0,9 MΩ**

Diese Werte werden in die FI-/Fehlerbewertung integriert.

---

## Wichtige Klarstellung zu deinen Messungen

### 1) Schutzleiterwiderstand 0,06 Ω
Das ist ein **guter** Wert – aber er sagt nur:

- PE ist sauber mit dem Gehäuse verbunden (niederohmige Verbindung)

Er sagt **nicht**, dass es **keinen** Isolationsfehler L/N→Gehäuse gibt.  
Der Schutzleiterwiderstand ist eine **Durchgangsprüfung**, keine Isolationsprüfung.

### 2) Isolationswiderstand Heizelement ~0,9 MΩ
Das ist **nicht „top“**, aber auch **nicht automatisch FI-auslösend**.

Grobe Einordnung bei 230 V (rein ohmisch gedacht):
- I ≈ 230 V / 0,9 MΩ ≈ **0,26 mA**

Damit liegt man deutlich unter 30 mA → das allein sollte einen 30‑mA‑FI nicht sofort auslösen.

**Aber:** In der Praxis ist Leckstrom oft **nicht rein ohmisch** (Feuchte, Kriechwege, kapazitive Pfade, Nichtlinearitäten).  
Außerdem kann es **weitere Leckpfade** geben (Kabelbaum, Motor, Netzeingang, MOV), die additiv wirken.

---

## Kontext / Randbedingungen

- Gerät Schutzklasse I
- **PE ist nur am Gehäuse**, nicht am PowerBoard geführt
- FI löst „beim Einstecken“ aus (also ohne aktive Schaltvorgänge)

**Konsequenz:** Auf dem PowerBoard gibt es keinen klassischen Y‑Cap‑Pfad nach PE. FI-Trip entsteht typischerweise durch:

- L/N → Gehäuse (Kabel/Lasten/Kriechweg)
- nichtlineares Netzbauteil (MOV) in Kombination mit Gehäusepfaden

---

## a) Bildanalyse (Bottomside – funktionale Blöcke)

### A1: 230V-Input / Netzeingang (oben mittig)
- **AC-L / AC-N Pads**
- **T3.15A** (Sicherung)
- **RX** (Serien-/Sicherungswiderstand; entspricht FR1 in Referenz)
- **MOV 14D471** (parallel zum Netz)

### A2: PN8034-Netzteilblock (rechts oben)
- **PD1+PD2/PD4**: Diodenpfade im Primärbereich (entspricht D1/D4)
- **EC2+EC3 4.7 µF/450 V**: HV-Zwischenkreis (entspricht C1/C2)
- **PCC1 0.22 µF/275 V**: X2-Entstörung (L↔N)
- **PEC1 50 V / 1 µF**: Sekundär-/Hilfskondensator

### A3: TRIAC/Lastpfade (links)
- Pads: **HEATER, FAN, LAMP, MOTOR, FAN_L**
- großflächige Leiterbahnen (Strompfade)
- „TRIAC“-Zone

### A4: TRIAC-Ansteuerlogik (unten links)
SMD-Widerstände/Transistoren/Diode(n) für Gate-/Trigger-Ansteuerung.

### A5: 12‑pol MCU‑Connector (unten)
**Wichtiges Update:**
- **Pin 2 = 5V+** → versorgt die angeschlossene MCU/Display-Elektronik.
- weitere Signale: FAN/Lamp/Motor/Fan_L/DOOR etc.
- außerdem **GND** und **FAN12V**

**Konsequenz für dein früheres Reboot-Problem:**
Wenn die externe MCU über Pin2 (5V) gespeist wird, dann ist jede Instabilität/Spannungseinbruch auf der 5‑V‑Schiene sofort als Reset sichtbar – auch wenn ein Multimeter es nicht schnell genug sieht.

---

## b) Kondensatoren auf der 230V-Seite – Lokalisierung & FI-Relevanz

| PCB | Wert | Typ | Anschluss | Referenz (Bild 3) | FI-Relevanz |
|---|---:|---|---|---|---|
| **PCC1** | 0.22 µF / 275 V | X2 | L↔N | CX (Entstörung) | ❌ |
| **EC2+EC3** | 4.7 µF / 450 V | HV-Elko | DC-Zwischenkreis | C1 + C2 | ❌ |
| **PEC1** | 1 µF / 50 V | Elko/Cap | Sekundär | nahe C3 | ❌ |

**Y‑Caps:** nicht erkennbar und ohne PE-Anbindung am Board unlogisch → als Board-Ursache sehr unwahrscheinlich.

---

## c) PVR1 – 14D471K (MOV)

Varistor parallel zum Netz (L↔N).  
Gealtert/teildefekt kann er:
- nichtlinear „anlecken“
- kurze Stromspitzen erzeugen
- thermisch instabil sein

➡️ Sinnvoller Ausschlusstest: **ein Bein hoch / auslöten (Diagnose)**.

---

## d) 5TET3.15A / 250 V

Netzsicherung, träge. Kein typischer FI-Auslöser.

---

## e) PCC1 – 275 V / 0.22 µF

X2-Kondensator L↔N. EMV. Kein FI-Pfad nach PE.

---

## f) PE nur am Gehäuse – was bleibt als FI-Ursache?

Wenn FI „sofort“ auslöst, sind typische Kandidaten:

1) **Heizelement** (0,9 MΩ ist nicht perfekt; unter Feuchte/Temperatur kann es deutlich schlechter werden)  
2) **Motor/Fan** (Wicklung/Entstörglied/Feuchte)  
3) **Netzkabel/Zugentlastung/Kabelbaum** (Scheuerstelle am Blech)  
4) **Kriechwege** auf Board oder an Steckern/Schraubpunkten (Schmutz/verkohlte Stellen)  
5) **MOV** (PVR1) als Nichtlinearitäts-/Leck-Kandidat

---

## g) RX – wozu?

Entspricht funktional **FR1 22 Ω / 2 W** aus Referenz:
- Inrush-/Stromspitzenbegrenzung
- Dämpfung von Ringing
- teilweise „fusible“ Charakter (Schutz)

---

## h) L1 + L2 – wozu?

- **L1 (1 mH)**: Primär-EMV/Seriendrossel (Störungen Richtung Netz reduzieren)
- **L2 (EE10 / 1.6 mH)**: Energietransfer/Glättung für die 12‑V‑Erzeugung

---

## i) Relais (HEATER)

Relais schaltet den Heater (230 V). Spule wird über Steuerpin vom 12‑pol Connector angesteuert.

**Für FI beim Einstecken:** Relais selbst ist selten Ursache – eher Heizkreis/Kabelbaum/Heizelement.

---

## Zuordnung: PN8034‑Referenz (Bild 3) ↔ Board (Bild 1/2)

| Referenz (Bild 3) | Board (Bild 1/2) | Kommentar |
|---|---|---|
| MOV1 10D471 | **PVR1 14D471K** | gleiche Funktion |
| FR1 22Ω/2W | **RX** | Serien-/Sicherungswiderstand |
| C1/C2 4.7µF/400V | **EC2+EC3 4.7µF/450V** | HV-Zwischenkreis |
| D1/D4 1N4007 | **PD1/PD2/PD4** | Diodenpfade |
| L1 1mH | **L1** | Primär-EMV |
| U1 PN8034 | **PN8034N** | SMPS |
| L2 1.6mH EE10 | **L2** | Energietransfer/Glättung |
| C4 470µF/25V | 12‑V‑Pufferelko | 12‑V‑Schiene |
| (nicht in Ref) 7805 | **7805** | 12V→5V für MCU-Versorgung (Pin2 5V+) |

---

## Praktische nächste Schritte (konkret und effizient)

### Für FI-Problem
1) **Isolationsmessung L→Gehäuse und N→Gehäuse** (wenn möglich komponentenweise abklemmen: Heater/Motor/Fan)  
2) **Heater warm/feucht testen** (Heizelemente fallen oft erst bei Wärme/Feuchte im Isowert ab)  
3) **MOV PVR1 Ausschlusstest** (ein Bein hoch)  
4) Sichtprüfung/Abstand: Netzkabel, Zugentlastung, Durchführungen, Blechkanten

### Für 5V/MCU-Stabilität (dein früheres Reboot-Thema)
Da **Pin2=5V+** die MCU speist:
- 5‑V‑Ripple/Drop mit Oszi messen (Multimeter ist zu langsam)
- 5‑V‑Leitung zur MCU kurz halten, zusätzliche Pufferung direkt an MCU (Low‑ESR Elko + 100 nF)

---

*Dokument aktualisiert mit Pin2=5V+ (MCU-Versorgung) und deinen Messwerten (PE 0,06 Ω; Heater ISO ~0,9 MΩ).*  
