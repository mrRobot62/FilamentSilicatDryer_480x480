# PowerBoard 230 V – Vollanalyse & Zuordnung zur PN8034-Referenzschaltung (Topside + Bottomside)

![🔗 Bild 1 – PowerBoard Topside (beschriftet)](PowerBoard_TopSide_230v_TopSide.png)  
![🔗 Bild 2 – PowerBoard Bottomside (230-V-Bereich)](PowerPCB_BottomSide_230V.png)  
![🔗 Bild 3 – PN8034 Referenzschaltung (Chipown)](PN8034_wiring.png)

---

## Kontext / wichtigste Randbedingung

- Gerät ist **Schutzklasse I**, **PE hängt nur am Gehäuse**
- **Keine elektrische PE-Verbindung zum PowerBoard** (keine PE-Leiterbahn/kein PE-Pin am Board)

**Konsequenz:**  
Ein FI (RCD) kann hier **nicht** „klassisch“ durch Y‑Kondensator-Ableitstrom (L/N → PE) auf dem PowerBoard auslösen, weil es auf dem Board keinen PE‑Bezug gibt.  
Ein FI-Trip „beim Einstecken“ kommt in so einem Aufbau praktisch immer von:

- **Isolationsfehler L oder N → Gehäuse (PE)** (Kabelbaum, Scheuerstelle, Schraube, Kriechweg)
- **nichtlinearem/lecken Verhalten von Netzbauteilen** (z. B. MOV), das in Kombination mit Gehäusepfaden/Schmutz/Feuchte relevant wird

---

## a) Bildanalyse (funktionale Blöcke)

### Bottomside (Bild 1)
- Links: **Leistungsteil / 230-V-Lastpfade** (HEATER, Fan, Lamp, Motor, FAN-L) und ein **Treiberfeld** (SMD‑Transistoren/Widerstände), das vermutlich die TRIAC‑Gates/Opto‑Stufen ansteuert.
- Mitte/oben: **Netzeingang** („220V IN“), Sicherung, MOV, RX, Diodenfeld.
- Rechts: **PN8034‑Netzteilbereich** (PD‑Dioden, Elkos, Primär-/Sekundärpfade, 12‑V/5‑V‑Versorgung).

### Topside (Bild 2)
- Sichtbar: PN8034N, HV‑Elkos (EC2/EC3), X‑Kondensator (PCC1), Sicherung, MOV, Relais, TRIAC‑Leistungsteil, 7805.

---

## b) „Y‑Caps“ bzw. FI‑kritische Kondensatoren lokalisieren

### Was auf den Fotos sichtbar ist
- **PCC1 0.22 µF / 275 V**: typischer **X2‑Kondensator** (L ↔ N)
- **EC2 + EC3 4.7 µF / 450 V**: HV‑Elkos im Zwischenkreis (nach Gleichrichtung)
- Diverse kleine Keramik‑/SMD‑Cs im Steuerteil (nicht als Safety‑Y erkennbar)

### Ergebnis (Topside + Bottomside)
- **Kein klassischer Y‑Kondensator (Y1/Y2) erkennbar**, der von L oder N auf PE (Gehäuse) geht.
- Das passt zu deiner Info „PE nur am Gehäuse“.

**Wichtig:**  
Wenn das Gerät trotzdem einen FI sofort wirft, ist der Fehler sehr wahrscheinlich **nicht** „Y‑Cap defekt“, sondern ein **L/N→Gehäuse‑Isolationsproblem** oder ein **nichtlinearer Netzbauteilfehler**.

---

## c) PVR1 – 14D471K

**Typ:** MOV/Varistor (Metal Oxide Varistor)  
**Funktion:** Überspannungsschutz (Netztransienten), typisch **L ↔ N** parallel.

- „14D“: ca. 14 mm Scheibe  
- „471“: Varistorspannung ~470 V  
- „K“: Toleranz

**FI-Bezug:**  
Ein MOV ist nicht „gegen PE“, aber ein gealterter MOV kann:
- deutlich mehr Leckstrom / nichtlineares Verhalten zeigen
- unter Netzspannung „anlecken“ oder kurzzeitig leitend werden

➡️ In der Praxis ein **häufiger Kandidat** bei „komischem Verhalten beim Einstecken“.

---

## d) „5TET3.15A / 250 V“

**Typ:** Netzsicherung, **träge T3.15A**, 250 V.  
Schützt gegen Überstrom/Kurzschluss.

**FI-Bezug:** keine direkte Ursache; sie sitzt nur im Netzeingangspfad.

---

## e) PCC1 – 275 V / 0.22 µF

Sehr wahrscheinlich **X2‑Entstörkondensator** (L ↔ N).  
- EMV‑Reduktion, Funken-/Schaltflankendämpfung
- **kein** PE‑Bezug ⇒ **kein** klassischer FI‑Leckpfad

---

## f) PE nur am Gehäuse (kein PE am Board)

**Konsequenz für Fehlersuche:**
- Suche primär nach **L/N→Gehäuse**:
  - Netzkabel/Zugentlastung
  - Kabelbaum zu Heizer/Motor/Fan
  - Scheuerstellen am Blech
  - Schrauben/Befestigungspunkte (Kriechweg von Leiterbahn zu Schrauböse)
  - verschmutzte/verkokelte Bereiche (Carbon tracking)

---

## g) RX – was ist das und wofür?

Im PN8034‑Referenz (Bild 3) heißt das Element **FR1 22 Ω / 2 W**: ein Serienwiderstand im Netzeingangspfad (häufig **fusible resistor** / Sicherungswiderstand).

Auf deinem PCB ist das als **RX** markiert (Bild 1/2).

**Aufgaben:**
- Stromspitzen-/Inrush‑Begrenzung (zusammen mit evtl. NTC)
- Dämpfung von Schwingungen/Spikes im Primärpfad
- „Opferbauteil“ bei Fehlbedingungen (soll definierter sterben als Leiterbahn/IC)

---

## h) L1 + L2 – wofür werden sie genutzt?

Aus Bild 3 (Referenz):

- **L1 (1 mH)**: Serieninduktivität im Primärpfad  
  → reduziert leitungsgebundene Störungen und dämpft PN8034‑Stromimpulse Richtung Netz.

- **L2 (1.6 mH, EE10)**: Induktives Energietransfer-/Glättungselement im Sekundärpfad (je nach PN8034‑Topologie als gekoppelte Drossel/Trafo‑ähnliches Element).  
  → glättet/überträgt Energie für die **12‑V‑Rail**.

**Zuordnung auf deinem PCB:**
- **L1**: im Topside-Foto als „L1“ beschrifteter Bereich nahe PN8034‑Zone (serielle Induktivität/Choke).
- **L2**: das markierte Induktivelement („L2, Blau‑Braun‑Rot“ in deiner Topside‑Beschriftung) entspricht funktional dem EE10‑Element der Referenz.

---

## i) Relais für HEATER (Ansteuerung über 12‑poligen Connector)

Das Relais schaltet den Heizkreis (230 V) und wird durch das Steuerboard über den 12‑poligen Connector angesteuert (Spule typischerweise 12 V).

**FI-Relevanz:**  
Wenn der FI **sofort beim Einstecken** fliegt (ohne dass die MCU überhaupt „wach“ ist), ist das Relais als Auslöser eher unwahrscheinlich – außer es gibt einen Isolationsfehler im Heizkreis/Kabelbaum zum Gehäuse.

---

## Zuordnung: PN8034‑Referenz (Bild 3) ↔ PowerBoard (Bild 1/2)

| Referenz (Bild 3) | Funktion | PowerBoard (Bild 1/2) |
|---|---|---|
| MOV1 10D471 | Überspannungsschutz L↔N | **PVR1 14D471K** |
| FR1 22Ω/2W | Serien-/Sicherungswiderstand | **RX** (PCB-Markierung) |
| CX1 0.1µF/250V | HF-Bypass L↔N | kleine Folie/Kerko nahe Netzeingang (nicht immer eindeutig sichtbar) |
| C1/C2 4.7µF/400V | HV-Zwischenkreis | **EC2 + EC3 4.7µF/450V** |
| D1/D4 1N4007 | Gleichrichterpfad | **PD1/PD2/PD4** (Diodenfeld) |
| L1 1mH | Primär-EMV-Induktivität | **L1** (PCB-Beschriftung) |
| U1 PN8034 | SMPS-Controller | **PN8034N** |
| L2 1.6mH EE10 | Sekundär-Energieelement | **L2** (Induktivelement) |
| D3 ES1J | Sekundärgleichrichtung | Diode im 12-V-Bereich (auf PCB nahe 12-V-Elko) |
| C4 470µF/25V | 12-V-Puffer | 470µF/25V-Elko im 12-V-Bereich |
| 7805 (nicht in Referenz) | 5-V-Regler | **U1 7805** (Topside) |

---

## Praktische FI-Interpretation aus den Bildern

Da keine Y‑Caps nach PE erkennbar sind und PE nicht am Board liegt:

1. **Isolationsmessung L→Gehäuse und N→Gehäuse @ 500 V** (mit Metratester) ist die wichtigste Messung.  
2. **MOV PVR1 testweise abtrennen** (ein Bein hoch / auslöten) ist ein schneller Ausschlusstest.  
3. **Wackeltest** (Netzkabel/Kabelbaum bewegen) während Isomessung findet Scheuerstellen zuverlässig.

---

*Dokument erstellt aus den bereitgestellten Bildern (Topside + Bottomside) und der PN8034‑Referenzschaltung.*  
