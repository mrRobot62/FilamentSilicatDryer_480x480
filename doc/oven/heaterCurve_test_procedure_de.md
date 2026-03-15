# T15 HeaterCurve – Testvorgehen zur empirischen Parameterermittlung

## Ziel dieses Dokuments

Dieses Dokument beschreibt die **empirische Vorgehensweise**, mit der die entscheidenden HeaterCurve-Parameter bestimmt werden sollen.

Wichtigster Grundsatz:

> **Nie mehrere Regelparameter gleichzeitig verändern.**

Nur wenn immer **genau ein Parameterblock** verändert wird, kann später nachvollzogen werden:

- welcher Parameter welchen Einfluss hat
- welche Wechselwirkung zwischen den Parametern besteht
- wie die Parameter sauber aufeinander abgestimmt werden

---

# Grundprinzip der Parametrierung

Die spätere Heizkurve hängt im Wesentlichen von drei Gruppen von Parametern ab:

1. **thermische Systemparameter**
   - z. B. Nachlaufzeit (`tau`)
   - Ansprechverhalten der Chamber
   - Reaktionsgeschwindigkeit des Hotspots

2. **Sicherheits- und Frühwarnparameter**
   - z. B. `hotspot_limit`
   - `hotspot_delta_limit`

3. **Regelparameter**
   - z. B. HOLD-Duty
   - APPROACH-Abbremsung
   - Slope-Filter

Diese Parameter dürfen **nicht gleichzeitig** optimiert werden.

Die richtige Reihenfolge ist:

1. **System verstehen**
2. **Sicherheitsgrenzen festlegen**
3. **Regelverhalten abstimmen**
4. **Feintuning**

---

# Wichtige Regel

## Eine Testreihe = nur ein Ziel

Beispiel:

### erlaubt
- nur `tau` bestimmen
- nur `hotspot_limit` bestimmen
- nur `hold_duty` bestimmen

### nicht erlaubt
- gleichzeitig `tau`, `hotspot_limit` und `hold_duty` ändern

---

# Feste Randbedingungen für alle Tests

Damit Ergebnisse vergleichbar bleiben, müssen folgende Bedingungen pro Testserie möglichst konstant bleiben:

- gleiche Hardware
- gleiche Sensorposition
- gleiches Gehäuse / gleiche Türstellung
- gleiche Luftströmung / Lüfterzustand
- gleiche PWM-Frequenz
- gleiche Umgebungstemperatur
- möglichst gleiche Ausgangstemperatur vor Testbeginn

Zusätzlich sollte jeder Test dokumentieren:

- Datum / Uhrzeit
- Starttemperatur Chamber
- Starttemperatur Hotspot
- Türstatus
- verwendete Parameter
- Besonderheiten / Beobachtungen

---

# Empfohlene Reihenfolge der empirischen Tests

## Phase 1 – Sensorik und Messpfad verifizieren

Ziel:
- sicherstellen, dass alle Messwerte plausibel sind

Zu prüfen:
- Chamber zeigt bei Raumtemperatur plausible Werte
- Hotspot zeigt bei Raumtemperatur plausible Werte
- Handwärme / leichte Erwärmung erzeugt nachvollziehbare Änderung
- Door open / closed wird korrekt erkannt
- Heater OFF / ON ist im Log eindeutig sichtbar

### Ergebnis dieser Phase
Erst wenn diese Basis korrekt funktioniert, dürfen weitere Tests erfolgen.

---

## Phase 2 – Thermisches Grundverhalten des Systems bestimmen

Diese Phase dient dazu, das System selbst zu verstehen, **noch ohne echte Regelung**.

---

### Test 2.1 – Reine Aufheizreaktion

Ziel:
- verstehen, wie schnell Hotspot und Chamber bei konstanter Heizleistung reagieren

Vorgehen:
1. Chamber auf möglichst definierte Starttemperatur bringen
2. Heater mit fester Leistung einschalten
3. Temperaturen loggen
4. Test vor kritischen Bereichen beenden

Zu beobachten:
- wie schnell steigt Hotspot?
- wann beginnt Chamber messbar zu steigen?
- wie groß ist die zeitliche Verzögerung zwischen Hotspot und Chamber?
- wie groß wird `hotspot - chamber` während des Aufheizens?

### Ergebnis
Diese Messung liefert:
- erstes Gefühl für Systemträgheit
- erstes Gefühl für typische Hotspot-Deltas
- erste Abschätzung, ab wann Hotspot kritisch wird

**In dieser Phase noch keine Parameter anpassen.**

---

### Test 2.2 – Nachlauf nach Heater-OFF

Ziel:
- Bestimmung von `tau` bzw. der thermischen Nachlaufzeit

Vorgehen:
1. System bis in die Nähe des Zielbereichs aufheizen
2. Heater zu einem definierten Zeitpunkt abschalten
3. Temperaturen weiter loggen, bis Chamber ihr Maximum erreicht
4. Zeit und Temperaturanstieg nach Heater-OFF erfassen

Zu messen:
- Chamber bei Abschaltzeitpunkt
- Chamber-Maximum nach Abschalten
- Zeit bis zum Maximum
- Hotspot-Abfall nach Heater-OFF

Beispiel:

- Heater OFF bei 40.0°C Chamber
- Chamber steigt weiter bis 43.8°C
- Maximum nach 68 s erreicht

### Ergebnis
Daraus wird abgeleitet:

- wie stark Chamber typischerweise nachläuft
- wie lange der Nachlauf dauert
- daraus folgt ein **erster Schätzwert für `tau`**

Wichtig:
`tau` wird **vor** dem Hotspot-Limit sauber bestimmt, weil `tau` direkt beschreibt, wie aggressiv die Vorhersage arbeiten muss.

---

# Warum tau zuerst bestimmt werden muss

`hotspot_limit` ist kein isolierter Parameter.

Wenn `tau` zu klein gewählt wird:
- der Algorithmus unterschätzt den Nachlauf
- man würde ein künstlich zu niedriges Hotspot-Limit brauchen

Wenn `tau` zu groß gewählt wird:
- der Algorithmus wird zu vorsichtig
- man würde ein unnötig hohes oder instabiles Hotspot-Limit ableiten

Deshalb gilt:

> **Zuerst tau bestimmen, danach erst hotspot_limit abstimmen.**

---

## Phase 3 – Sicherheits- und Frühwarnparameter bestimmen

---

### Test 3.1 – Hotspot-Verhalten bei bekanntem tau

Voraussetzung:
- `tau` ist bereits grob bekannt

Ziel:
- Hotspot als Frühwarnindikator kalibrieren

Vorgehen:
1. mit festem `tau` arbeiten
2. Chamber auf mehrere Zielbereiche fahren
3. bei jeder Fahrt beobachten:
   - wie hoch wird Hotspot?
   - wie groß wird `hotspot - chamber`?
   - ab welchem Wert beginnt Chamber später zuverlässig zu überschwingen?

Zu erfassen:
- Hotspot absolut
- Hotspot-Delta
- resultierendes Chamber-Verhalten
- ob Overshoot auftritt

### Ergebnis
Daraus werden zwei Parameter abgeleitet:

#### a) `hotspot_limit`
Absolute Hotspot-Grenze

#### b) `hotspot_delta_limit`
Grenze für:

`hotspot - chamber`

Wichtig:
Erst wenn `tau` schon sinnvoll gewählt wurde, haben diese beiden Werte echte Aussagekraft.

---

### Test 3.2 – Sicherheitsgrenze konservativ festlegen

Nachdem erste Hotspot-Messungen vorliegen, wird zunächst ein **konservativer Default** gesetzt.

Beispiel:

- beobachteter problematischer Bereich: 95°C Hotspot
- erster Arbeitswert:
  `hotspot_limit = 90°C`

Analog für Delta:

- problematisch ab 32°C
- erster Arbeitswert:
  `hotspot_delta_limit = 25°C`

### Ziel
Nicht gleich „perfekt optimieren“, sondern erst einmal einen **sicheren Startwert** festlegen.

---

## Phase 4 – Regelparameter im Zielbereich bestimmen

Erst jetzt wird die eigentliche Regelung fein abgestimmt.

---

### Test 4.1 – APPROACH-Verhalten

Ziel:
- bestimmen, wie früh und wie stark vor dem Ziel gebremst werden muss

Vorgehen:
1. `tau` und Hotspot-Limits bleiben fix
2. Start deutlich unter Ziel
3. Regelung läuft in den Zielbereich
4. nur APPROACH-bezogene Parameter anpassen

Zu beobachten:
- wird zu spät abgeschaltet?
- wird zu früh abgeschaltet?
- bleibt die Chamber unter der Obergrenze?
- fällt sie unnötig weit unter den Zielbereich zurück?

### Typische Stellgrößen
- APPROACH duty scaling
- APPROACH threshold
- predictive cutoff aggressiveness

Wichtig:
In dieser Phase **keine Änderung an tau oder hotspot_limit**.

---

### Test 4.2 – HOLD-Verhalten

Ziel:
- stabile Temperaturhaltung im Zielband

Vorgehen:
1. `tau`, Hotspot-Limits und APPROACH bleiben fix
2. System in den Zielbereich bringen
3. HOLD-Modus über längere Zeit beobachten

Zu beobachten:
- wie stark pendelt die Chamber?
- wie oft wird nachgeheizt?
- wie hoch muss die minimale Heizleistung sein?
- wird das Zielband sicher eingehalten?

### Typische Stellgrößen
- `holdWindowMs`
- `minDutyHoldPct`
- `maxDutyHoldPct`

Wichtig:
Hier keine Änderung mehr an den vorher bestimmten Parametern.

---

## Phase 5 – Gesamtsystem validieren

Erst wenn alle Einzelparameterblöcke sinnvoll bestimmt wurden, erfolgt die kombinierte Validierung.

Ziel:
- prüfen, ob das System als Ganzes stabil arbeitet

Testarten:
1. Kaltstart auf Zieltemperatur
2. Wiederanlauf aus warmem Zustand
3. längeres Halten
4. Door open während laufender Regelung
5. Test verschiedener Zieltemperaturen

### Bewertung
Das System ist gut abgestimmt, wenn:

- das Zielband zuverlässig eingehalten wird
- Overshoot innerhalb der Vorgabe bleibt
- keine unnötigen starken Oszillationen auftreten
- Hotspot unter Kontrolle bleibt
- Safety-Verhalten reproduzierbar ist

---

# Konkrete Reihenfolge der Parameterfestlegung

Die Parameter sollten in genau dieser Reihenfolge bestimmt werden:

## Block A – nicht ändern, nur beobachten
- Sensorik
- Rohwerte
- Temperaturplausibilität
- Hotspot/Chamber Verzögerung

## Block B – zuerst bestimmen
- `tau`

## Block C – danach bestimmen
- `hotspot_limit`
- `hotspot_delta_limit`

## Block D – danach bestimmen
- `slope_filter_alpha`

## Block E – danach bestimmen
- APPROACH-Parameter

## Block F – zuletzt bestimmen
- HOLD-Parameter
  - `holdWindowMs`
  - `minDutyHoldPct`
  - `maxDutyHoldPct`

---

# Warum diese Reihenfolge wichtig ist

Wenn man z. B. zuerst `hotspot_limit` tuned und `tau` später ändert, dann ist der alte Hotspot-Wert praktisch wertlos, weil sich die Vorhersagelogik geändert hat.

Ebenso gilt:
- wenn HOLD noch nicht stabil ist, kann man APPROACH schlecht bewerten
- wenn Hotspot-Limits unklar sind, kann man die eigentliche Regelung nicht sinnvoll abstimmen

Darum ist die Reihenfolge zwingend.

---

# Praktische Testempfehlung

Für jeden Testlauf sollte eine kurze Test-ID vergeben werden.

Beispiel:

`T15-Run-001`

Dokumentieren:

- Zieltemperatur
- Starttemperatur
- aktive Parameter
- Testzweck
- Beobachtung
- Ergebnis

Beispiel:

## T15-Run-014
- Ziel: tau bestimmen
- Start: 22°C
- Heater fest eingeschaltet bis 40°C
- Heater OFF bei 40°C
- Peak: 43.6°C nach 64 s

Ergebnis:
- erster tau-Arbeitswert: 60 s

---

# Logging für die Parameterermittlung

Für jeden Lauf sollten mindestens diese Werte geloggt werden:

- Zeitstempel
- Door Zustand
- Chamber Temperatur
- Hotspot Temperatur
- Heater Duty
- Controller Mode
- Chamber Slope
- Predicted Chamber Temperatur
- Safety Reasons

Ergänzend hilfreich:
- raw ADC
- mV
- Ohm

---

# Empfohlene Vorgehensweise für den Start

## Startwert-Strategie

Beim ersten aktiven Test:

1. konservativen `tau` setzen
2. konservatives `hotspot_limit` setzen
3. konservatives `hotspot_delta_limit` setzen
4. Regelung vorsichtig starten
5. nur beobachten
6. dann anhand der Logs gezielt einen Parameterblock anpassen

Beispiel Startwerte:
- `tau = 60 s`
- `hotspot_limit = 90°C`
- `hotspot_delta_limit = 25°C`

Diese Werte sind **nicht final**, sondern nur sichere erste Arbeitswerte.

---

# Zusammenfassung

Die wichtigste Regel für T15 lautet:

> **Zuerst das System verstehen, dann die Sicherheitsgrenzen festlegen, erst danach die eigentliche Regelung abstimmen.**

Die korrekte Reihenfolge ist:

1. Sensorik verifizieren
2. thermischen Nachlauf (`tau`) bestimmen
3. Hotspot-Grenzen bestimmen
4. Slope-Filter bestimmen
5. APPROACH abstimmen
6. HOLD abstimmen
7. Gesamtsystem validieren

So bleibt jederzeit nachvollziehbar, welcher Parameter welchen Einfluss auf die Heizkurve und das Overshoot-Verhalten hat.
