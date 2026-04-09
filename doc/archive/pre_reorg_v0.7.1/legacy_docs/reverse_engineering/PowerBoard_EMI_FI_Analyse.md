# PowerBoard 230V – EMI / FI Analyse
![PowerBoard](PowerBoard_TopSide_230v_TopSide.png)
---

## a) Allgemeine Analyse des PowerBoards (230V-Seite)

Das PowerBoard zeigt eine **klassische Haushaltsgeräte-Topologie ohne galvanisch angebundenen Schutzleiter (PE)**:

**Netzpfad (vereinfacht):**
```
AC-L / AC-N
  │
  ├─ Sicherung (T3.15A)
  ├─ Varistor (MOV)
  ├─ X2-Kondensator (L–N)
  ├─ Primärnetzteil (PN8034N)
  └─ Lasten (Heater, Fan, Motor, Lampe)
```

Der **Schutzleiter (PE)** ist **ausschließlich mechanisch mit dem Metallgehäuse verbunden**  
→ **keine elektrische Verbindung zum PowerBoard**.

Damit existiert:
- ❌ kein klassischer EMI-Filter mit Y-Kondensatoren (L/N → PE)
- ❌ kein definierter Ableitstrompfad vom Board nach PE

Ein FI kann hier **nur** auslösen, wenn:
- ein **Isolationsfehler L/N → Gehäuse (PE)** vorliegt
- oder ein **nichtlinearer Hochspannungs-Leckpfad** (z. B. MOV, Verschmutzung, Kriechweg)

---

## b) Kondensatoren auf der 230V-Seite (FI-relevant?)

### Identifizierte Kondensatoren

| Bezeichnung | Typ | Position | Bewertung |
|------------|-----|----------|-----------|
| **PCC1** | X2-Kondensator | Zwischen L und N | **Nicht FI-relevant** |
| **EC2 / EC3** | Elko 450V | DC-Zwischenkreis | Nicht FI-relevant |
| **EC4** | Elko 50V | Niederspannung | Nicht FI-relevant |
| **EC1 / EC5** | Elko 16V | Logikversorgung | Nicht FI-relevant |

### Ergebnis
👉 **Es sind keine Y-Kondensatoren vorhanden.**  
Damit scheidet die klassische „Y-Cap → PE“-FI-Ursache vollständig aus.

---

## c) Bauteil PVR1 – 14D471K

**Typ:** MOV (Metal Oxide Varistor)

**Daten:**
- Scheibendurchmesser: ~14 mm
- Nennspannung: 470 V
- Anschluss: **L ↔ N**

**Funktion:**
- Überspannungsschutz (Netztransienten, Schaltspitzen)

**FI-Relevanz:**
- Normal: hochohmig
- Defekt/gealtert:
  - wird nichtlinear leitend
  - kann sehr hohe Ströme ziehen
  - **kann FI-Auslösung begünstigen**, auch ohne PE-Anbindung

➡️ **Top-Verdächtiger für „FI fliegt sofort beim Einstecken“**

---

## d) Bauteil „5TET3.15A 250V“

**Typ:** Netzsicherung

**Daten:**
- 3.15 A
- träge (T)
- 250 V

**Funktion:**
- Überstromschutz

**FI-Relevanz:**  
❌ Keine direkte FI-Auslösung  
✔️ Bestätigt aber: Der Fehler liegt **nach** der Sicherung.

---

## e) Bauteil PCC1 – 275V, 0.22µF

**Typ:** X2-Entstörkondensator

**Anschluss:** L ↔ N

**Funktion:**
- HF-Entstörung
- Funkenlöschung

**FI-Relevanz:**  
❌ Keine – da kein Bezug zu PE  
Defektfall würde eher Sicherung/LS auslösen.

---

## f) Schutzleiter (PE) – Bedeutung im vorliegenden Gerät

- PE ist **nur mit dem Metallgehäuse verbunden**
- Keine Leiterbahn, kein Bauteil vom PowerBoard zu PE

**Konsequenzen:**
- Kein definierter Ableitstrom über das Board
- FI-Auslösung nur möglich durch:
  - L/N → Gehäuse-Isolationsfehler
  - Kriechstrecken / Verschmutzung
  - mechanische Beschädigung
  - oder nichtlinear leitenden MOV

---

## Gesamtfazit

### Ausgeschlossen
- Y-Kondensator-Defekt
- X2-Kondensator
- Niedervolt-Elkos
- Logik-/MCU-Bereich

### Sehr wahrscheinlich
1. **MOV PVR1 (14D471K) gealtert oder teildefekt**
2. **Isolationsfehler L/N → Gehäuse**
3. **Kabelbaum / Netzanschluss / Zugentlastung**
4. **Kriechweg Board → Befestigungsschraube → Gehäuse**

---

## Empfohlene nächste Schritte

1. **MOV testweise auslöten** (nur Diagnose!)
2. **Isolationsmessung L → Gehäuse und N → Gehäuse @ 500V**
3. **Wackeltest** während Iso-Messung
4. Board testweise **mechanisch vom Gehäuse isolieren**

---

*Dokument erstellt zur systematischen FI-/EMI-Analyse eines SK-I-Geräts ohne Y-Kondensatoren.*
