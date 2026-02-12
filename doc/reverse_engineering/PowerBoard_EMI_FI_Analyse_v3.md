# PowerBoard 230 V – EMI / FI / Netzteil-Analyse (PN8034-basiert)

<img src="PowerBoard_TopSide_230v_TopSide.png" width="100%">
---
<img src="PN8034_wiring.png" width="100%">
---

## a) Gesamtanalyse des PowerBoards (230-V-Seite)

Das PowerBoard entspricht einer **typischen kosteneffizienten Haushaltsgeräte-Architektur**:

- ungeregelter Netzeingang (AC-L / AC-N)
- diskrete Netz- und EMV-Beschaltung
- integriertes primäres Schaltnetzteil auf Basis **PN8034**
- galvanisch **nicht getrennte** 12-V-Hilfsspannung
- Lastschaltung über **Relais (Heater)** und **TRIACs (Motor/Fans/Lampe)**

Der Schutzleiter (**PE**) ist **ausschließlich mechanisch mit dem Metallgehäuse verbunden**.

> Es existiert **keine elektrische Verbindung zwischen PE und dem PowerBoard**.

---

## b) Kondensatoren auf der 230-V-Seite

### Identifizierte Kondensatoren

| Bezeichnung | Wert / Typ | Anschluss | Funktion | FI-Relevanz |
|------------|------------|-----------|----------|-------------|
| **PCC1** | 0.22 µF / 275 V (X2) | L ↔ N | EMV-Entstörung | ❌ |
| **CX1** | 0.1 µF / 250 V | L ↔ N | HF-Bypass | ❌ |
| **C1 / C2 (EC2/EC3)** | 4.7 µF / 400–450 V | DC-Zwischenkreis | Primärpuffer | ❌ |
| **C3 (EC4)** | 4.7 µF / 50 V | Sekundär | Glättung | ❌ |
| **C4** | 470 µF / 25 V | 12-V-Rail | Pufferung | ❌ |

### Ergebnis

👉 **Keine Y-Kondensatoren vorhanden**  
→ kein definierter Ableitstrompfad nach PE  
→ klassischer FI-Auslöser durch Y-Caps ausgeschlossen

---

## c) Bauteil PVR1 – 14D471K

**Typ:** MOV (Metal Oxide Varistor)

- Anschluss: **L ↔ N**
- Varistorspannung: ca. 470 V
- Funktion: Überspannungsschutz (Netztransienten)

**Bewertung:**
- Normal: hochohmig
- Alterung/Defekt:
  - nichtlinearer Leckstrom
  - thermische Vorschädigung
  - kann FI-Auslösung **indirekt** begünstigen

➡️ **Sehr relevanter Prüfkandidat**

---

## d) Bauteil „5TET3.15A / 250 V“

**Typ:** Netzsicherung

- Nennstrom: 3.15 A
- Charakteristik: träge (T)
- Funktion: Überstromschutz

**FI-Relevanz:** keine direkte

---

## e) Bauteil PCC1 – 275 V / 0.22 µF

**Typ:** X2-Folienkondensator

- Anschluss: L ↔ N
- Funktion: EMV-Entstörung

**FI-Relevanz:** keine

---

## f) Schutzleiter (PE)

- PE ausschließlich am Gehäuse
- keine Leiterbahn / kein Bauteil zum Board

**FI kann nur auslösen durch:**
- Isolationsfehler L/N → Gehäuse
- Kriechstrecken / Verschmutzung
- mechanische Beschädigung
- defekten MOV

---

## g) Bauteil RX (Widerstand, ca. 1–2 W)

**Typ:** Serien-/Dämpfungswiderstand im Primärnetzteil

**Funktionen:**
- Einschaltstrombegrenzung (zusammen mit NTC)
- Dämpfung von Schwingkreisen
- Schutz des PN8034 vor Stromspitzen

**FI-Relevanz:** gering, aber thermische Schäden möglich

---

## h) L1 und L2 – Spulen / Drosseln

### L1 (1 mH, Primärseite)
- Bestandteil des EMV-Filters
- Reduziert leitungsgebundene Störungen
- Glättet Stromimpulse des PN8034

### L2 (1.6 mH, Sekundärseite)
- Ausgangsdrossel des Flyback-/Buck-ähnlichen Wandlers
- Glättung der 12-V-Gleichspannung

**FI-Relevanz:** keine direkte

---

## i) Relais (12 V / 250 V)

- Schaltet den **HEATER**
- Spule: 12 V DC
- Ansteuerung über MCU-Signal vom 12-poligen Connector

**Wichtig:**
- Relais trennt den Heizkreis galvanisch vom Steuerteil
- kein Einfluss auf FI beim Einstecken

---

## Gesamtfazit

### Sicher ausgeschlossen
- Y-Kondensatoren
- X2-Kondensator PCC1
- Niedervolt-Elkos
- MCU-/Logikschaltung

### Wahrscheinlichste FI-Ursachen
1. **MOV PVR1 (14D471K) gealtert**
2. **Isolationsfehler L/N → Gehäuse**
3. **Netzleitung / Zugentlastung / Kabelbaum**
4. **Kriechweg Leiterbahn → Schraube → Gehäuse**

---

## Empfohlene Prüfungen

1. MOV testweise entfernen (Diagnose)
2. Isolationsmessung @ 500 V:
   - L → Gehäuse
   - N → Gehäuse
3. Wackeltest während Messung
4. Board mechanisch vom Gehäuse isolieren

---

*Analyse basierend auf Hersteller-Referenzschaltung PN8034 und realem PCB-Aufbau.*
