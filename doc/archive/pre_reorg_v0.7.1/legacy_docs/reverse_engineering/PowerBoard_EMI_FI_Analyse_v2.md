# PowerBoard 230 V – EMI- / FI-Analyse (basierend auf Foto)

<img src="PowerBoard_TopSide_230v_TopSide.png" width="100%">
---

## a) Gesamtanalyse des PowerBoards (230 V-Seite)

Das gezeigte PowerBoard entspricht einer **typischen Haushaltsgeräte-Topologie** mit:

- direktem Netzanschluss (L / N)
- diskret aufgebauter Entstörung
- integriertem Primär-Schaltnetzteil (PN8034N)
- Relais- und TRIAC-Lastansteuerung

Wichtiges Strukturmerkmal:

> **Der Schutzleiter (PE) ist ausschließlich mechanisch mit dem Metallgehäuse verbunden.  
> Es existiert keine elektrische Verbindung zwischen PE und dem PowerBoard.**

Damit gilt:
- ❌ kein klassischer Netz-EMI-Filter mit Y-Kondensatoren nach PE
- ❌ kein definierter Ableitstrompfad vom Board zum Schutzleiter

Ein FI (RCD) kann in diesem Aufbau **nur** auslösen, wenn ein **Isolationsfehler von L oder N zum Gehäuse (PE)** vorliegt.

---

## b) Kondensatoren auf der 230 V-Seite – Bewertung

### Identifizierte Kondensatoren

| Bezeichnung | Typ | Anschluss | Funktion | FI-Relevanz |
|------------|-----|-----------|----------|-------------|
| **PCC1** | X2-Folienkondensator | L ↔ N | Entstörung / HF-Dämpfung | ❌ |
| **EC2 / EC3** | Elko 450 V / 4,7 µF | DC-Zwischenkreis | Glättung Primärnetzteil | ❌ |
| **EC4** | Elko 50 V / 4,7 µF | Sekundär | Versorgung Buzzer | ❌ |
| **EC1 / EC5** | Elko 16 V / 470 µF | Niederspannung | Logikversorgung | ❌ |

### Ergebnis

👉 **Auf der 230‑V-Seite sind keine Y‑Kondensatoren vorhanden.**  
Damit ist ein klassischer „Y‑Cap‑Ableitstrom nach PE“ als FI-Ursache ausgeschlossen.

---

## c) Bauteil PVR1 – 14D471K

**Typ:** Varistor (MOV – Metal Oxide Varistor)

**Kenndaten:**
- Scheibendurchmesser: ca. 14 mm
- Nennspannung: 470 V
- Anschluss: **zwischen L und N**

**Funktion:**
- Schutz vor Überspannung und Schaltspitzen
- Klemmt transient hohe Spannungen

**FI-Relevanz:**
- Normalzustand: hochohmig
- Alterung / Vorschädigung:
  - nichtlinearer Leckstrom
  - thermische Vorschäden
  - kann **FI-Auslösung indirekt begünstigen**, auch ohne PE-Anbindung

➡️ **Hochprioritärer Prüfkandidat** bei „FI fliegt sofort beim Einstecken“.

---

## d) Bauteil „5TET3.15A / 250 V“

**Typ:** Netzsicherung

**Daten:**
- Nennstrom: 3,15 A
- Charakteristik: träge (T)
- Nennspannung: 250 V

**Funktion:**
- Überstrom- und Kurzschlussschutz

**FI-Relevanz:**
- ❌ keine direkte
- ✔️ zeigt: der Fehler liegt **nach** der Sicherung im Gerät

---

## e) Bauteil PCC1 – 275 V / 0,22 µF

**Typ:** X2-Entstörkondensator

**Anschluss:** L ↔ N

**Funktion:**
- Reduzierung von Schaltstörungen
- Funkentstörung

**FI-Relevanz:**
- ❌ keine (kein Bezug zu PE)
- Defekt würde eher Sicherung oder LS auslösen

---

## f) Schutzleiter (PE) – Bedeutung im vorliegenden Aufbau

- PE ist **nur am Metallgehäuse angeschlossen**
- keine Leiterbahn, kein Bauteil vom PowerBoard zum PE

### Konsequenzen

Ein FI kann nur auslösen durch:
- **Isolationsfehler L/N → Gehäuse**
- Kriechstrecken (Schmutz, Feuchte, Carbonisierung)
- mechanische Beschädigung (Kabel, Schrauben, Zugentlastung)
- Bauteile mit nichtlinearem Verhalten (z. B. MOV)

Nicht möglich:
- Y‑Kondensator‑Ableitstrom (nicht vorhanden)

---

## Gesamtfazit

### Sicher ausgeschlossen
- Y‑Kondensatoren
- X2‑Kondensator PCC1
- Niedervolt‑Elkos
- Logik‑ / MCU‑Schaltung

### Wahrscheinlichste Ursachen für FI-Auslösung
1. **Varistor PVR1 (14D471K) gealtert oder teildefekt**
2. **Isolationsfehler L oder N → Gehäuse**
3. **Kabelbaum / Netzeinführung / Zugentlastung**
4. **Kriechweg Leiterbahn → Befestigungsschraube → Gehäuse**

---

## Empfohlene nächste Schritte

1. **MOV PVR1 testweise entfernen** (nur Diagnose)
2. **Isolationsmessung @ 500 V DC:**
   - L → Gehäuse
   - N → Gehäuse
3. **Wackeltest** während der Isomessung
4. Board testweise **mechanisch vom Gehäuse isolieren**

---

*Diese Analyse basiert ausschließlich auf dem sichtbaren Aufbau des PowerBoards und dem bereitgestellten Foto.*
