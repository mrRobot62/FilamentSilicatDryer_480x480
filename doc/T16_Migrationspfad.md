# T16 Migrationspfad

## Ziel

Dieser Migrationspfad beschreibt den Weg vom aktuell historisch gewachsenen Softwarestand hin zu einer belastbaren, nutzbaren Produkt-Firmware fuer den FilamentSilicaDryer.

Er dient als gemeinsame Arbeitsgrundlage fuer kommende Iterationen.

---

## Ausgangslage

Aktueller Branch:
- `feature/T16_Host_Client_HeaterCurve`

Aktuelle Architektur:
- Host: ESP32-S3, UI, Runtime-State, Heizentscheidung
- Client: ESP32-WROOM, Sensorik, Aktoren, Safety-Gates, Status-Telemetrie
- Kommunikation: UART-basiertes ASCII-Protokoll zwischen Host und Client

Fachliche Kernziele:
- Filament sicher trocknen ohne kritisches Overshoot
- Silicagel mit hoeheren Temperaturen separat behandeln
- Chamber-Temperatur als fuehrende Regelgroesse
- Hotspot-Temperatur nur fuer Safety
- Relais-schonende ON/OFF-Steuerung ohne klassisches Leistungs-PWM

---

## Ist-Befund

Der aktuelle Stand ist keine Nullbasis mehr, sondern eine fortgeschrittene Zwischenstufe.

Bereits vorhanden:
- Dual-NTC-Sensorik im Client
- Protokoll mit Hotspot- und Chamber-Temperatur
- Host als Single Source of Truth fuer Runtime und Heizentscheidung
- Grundsaetzliche Safety-Mechanismen fuer Door, Comm-Loss und Temperaturgrenzen
- Funktionierende Hardware-Ansteuerung fuer Heater, Fans, Lamp und Motor

Noch problematisch:
- Altlogik aus T11/T15 ist im produktiven Code noch sichtbar
- Host-Regelpfad ist semantisch noch nicht komplett auf T16 bereinigt
- Heizlogik ist fachlich noch nicht sauber auf Filament- und Silica-Betrieb getrennt
- Teile der Dokumentation sind veraltet
- Build-Validierung ist aktuell noch nicht sauber in den Analyseablauf integriert

---

## Migrationsprinzipien

Diese Regeln gelten fuer alle naechsten Schritte:

1. Chamber ist die einzige Regelgroesse.
2. Hotspot dient ausschliesslich der Safety-Ueberwachung.
3. Heizlogik liegt ausschliesslich im Host.
4. Der Client bleibt fuer Sensorik, Safety-Gating und Aktor-Ausfuehrung zustaendig.
5. Keine Rueckkehr zu geschaetzten Chamber-Werten.
6. Keine vermischte Alt-/Neu-Architektur im produktiven Pfad.
7. Safety hat immer Vorrang vor UX oder Laufzeit.
8. Undershoot ist akzeptabler als Overshoot.
9. Relais-Schonung ist fester Bestandteil des Reglerdesigns.

---

## Zielbild

### Host
- klare, lesbare Heizlogik ohne T11-Altmodell
- materialabhaengige Regelparameter
- sauberer Runtime-State fuer UI und Diagnose
- explizite Safety-Zustaende und nachvollziehbare Entscheidungen

### Client
- stabile Erfassung beider NTCs
- robuste Status-Telemetrie
- rein technische Ausfuehrung von Host-Kommandos
- zusaetzliche lokale Notfall-Safety als letzte Instanz

### Systemverhalten
- keine unkontrollierten Overshoots bei Filament-Presets
- reproduzierbares Heizverhalten
- saubere Abkuehl- und WAIT/POST-Uebergaenge
- nachvollziehbares Logging fuer Testfahrten

---

## Phasenplan

## Phase 1: Produktiven Regelpfad bereinigen

### Ziel
Den Host-Code auf einen klaren T16-Stand bringen, ohne tote oder irrefuehrende Altlogik.

### Aufgaben
- T11/PT1-Reste im produktiven Host-Pfad identifizieren
- nicht mehr genutzte Legacy-Felder, Legacy-Kommentare und Altannahmen entfernen oder isolieren
- Runtime-State semantisch auf Chamber/Hotspot ausrichten
- Kommentare und Benennungen an die reale T16-Architektur anpassen

### Ergebnis
- `oven.cpp` und `oven.h` beschreiben nur noch die produktive Dual-NTC-Logik
- geringeres Risiko fuer Fehlentscheidungen bei Weiterentwicklung

---

## Phase 2: Heizentscheidung fachlich fixieren

### Ziel
Ein belastbares Host-Regelverhalten definieren, das Overshoot vermeidet und das Relais schont.

### Aufgaben
- Startverhalten des Heaters sauber definieren
- Hysterese und Safety-Schwellen zentral modellieren
- Mindest-EIN/AUS-Zeiten verbindlich integrieren
- Unterschied zwischen `heater_request_on` und `heater_actual_on` konsequent durchziehen
- unnoetige wiederholte `SET`-Frames vermeiden

### Ergebnis
- technisch stabile und nachvollziehbare Heizentscheidung
- reduzierte Kommunikationslast
- bessere Basis fuer reale Heiztests

---

## Phase 3: Temperatur-Policy pro Betriebsart

### Ziel
Filament-Trocknung und Silica-Trocknung fachlich sauber trennen.

### Aufgaben
- Presets fachlich kategorisieren
- Regeln fuer Filament-Betrieb definieren:
  - max. tolerierte Ueberschreitung
  - erlaubte Dauer einer Ueberschreitung
  - konservative Wiedereinschaltstrategie
- Regeln fuer Silica-Betrieb definieren:
  - hoeherer Zielbereich
  - tolerantere Ueberschreitung
  - eigene Safety- und Komfortparameter
- gemeinsame absolute Sicherheitsgrenzen beibehalten

### Ergebnis
- Presets bilden reale Materialanforderungen ab
- Filament wird konservativ geschuetzt
- Silica kann effektiver getrocknet werden

---

## Phase 4: Telemetrie, Logging und Diagnose schaerfen

### Ziel
Das Verhalten des Systems bei Testfahrten objektiv auswertbar machen.

### Aufgaben
- wichtige Reglerentscheidungen strukturiert loggen
- Temperaturverlauf Chamber vs. Hotspot sauber nachvollziehbar machen
- Safety-Ereignisse und Schaltvorgaenge eindeutig kennzeichnen
- Logging auf die T16-Zielarchitektur reduzieren

### Ergebnis
- echte Testdaten statt Bauchgefuehl
- bessere Grundlage fuer Regler-Tuning

---

## Phase 5: Reale Heizkurven validieren und tunen

### Ziel
Die Heizstrategie auf echter Hardware iterativ verbessern.

### Aufgaben
- definierte Testprozeduren je Preset oder Materialklasse aufstellen
- Aufheizverhalten, Overshoot und Ausschwingverhalten messen
- Parameter iterativ anpassen
- Abbruchkriterien fuer unsichere Testlaeufe festlegen

### Ergebnis
- belastbare Heizkurven statt theoretischer Annahmen
- reproduzierbares Verhalten fuer reale Nutzung

---

## Phase 6: Dokumentation auf Produktionsstand bringen

### Ziel
Nur noch Dokumentation pflegen, die dem realen Codezustand entspricht.

### Aufgaben
- veraltete Dokumente markieren oder archivieren
- Architektur, Zustandsmodell und Safety-Regeln neu dokumentieren
- Test- und Inbetriebnahmehinweise aktualisieren
- dieses Dokument nach jeder relevanten Iteration fortschreiben

### Ergebnis
- weniger Missverstaendnisse
- schnellerer Wiedereinstieg in spaeteren Sessions

---

## Konkrete Reihenfolge fuer die naechsten Iterationen

1. Host-Regelpfad bereinigen
2. offensichtliche Safety- und Zustandsfehler beheben
3. Heizentscheidung und Schwellwerte zentralisieren
4. redundante Kommandos und Legacy-Artefakte entfernen
5. Logging fuer Heiztests schaerfen
6. erste reale Testfahrt gegen PLA oder TPU vorbereiten
7. danach Materialklassen und Preset-Policy verfeinern

---

## Bekannte Risiken

- Historische Altlogik kann unbemerkt weiterwirken
- reale thermische Traegheit kann theoretische Schwellen unbrauchbar machen
- Sensorplatzierung kann trotz Dual-NTC weiterhin lokale Artefakte erzeugen
- Kommunikations-Sonderfaelle koennen Host- und Client-Zustand auseinanderziehen
- Build- oder Flash-Prozess kann Analyse und schnelle Iteration ausbremsen

---

## Definition of Done fuer T16

T16 ist fuer den produktiven Einsatz ausreichend abgeschlossen, wenn:

- Chamber-only-Regelung produktiv und eindeutig umgesetzt ist
- Hotspot-only-Safety stabil greift
- Relais-Schonung nachweisbar eingehalten wird
- Filament-Presets ohne kritisches Overshoot fahrbar sind
- Silica-Betrieb separat und sicher funktioniert
- Runtime-State, UI und Telemetrie konsistent sind
- Altlogik aus T11/T15 den produktiven Pfad nicht mehr verunklaert

---

## Arbeitsmodus fuer kommende Sessions

Bei jeder neuen Iteration:

1. zuerst dieses Dokument gegen den Codezustand abgleichen
2. den aktuellen Arbeitsschritt einer Phase zuordnen
3. nach Abschluss den Fortschritt hier nachziehen
4. neue Risiken oder Erkenntnisse hier ergaenzen

So bleibt der Migrationspfad dauerhaft die gemeinsame Referenz zwischen Analyse, Umsetzung und Test.
