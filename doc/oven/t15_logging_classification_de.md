# T15 Logging-Klassifizierung für empirische HeaterCurve-Tests

## Ziel

Die UDP-Logs sollen im Logviewer gezielt filterbar sein.

Dafür erhält jede relevante Nachricht einen festen Präfix:

`[T15-<testblock>]`

Beispiele:

- `[T15-BASE]`
- `[T15-2.1]`
- `[T15-2.2]`
- `[T15-3.1]`
- `[T15-4.2]`

So können im Viewer gezielt nur die Nachrichten eines bestimmten Testblocks angezeigt werden.

---

## Vorgabe für die Verwendung

### Allgemeine Basis-Logs
Für allgemeine Systemmeldungen ohne direkten Testbezug:

- `T15-BASE`

Beispiele:
- Boot
- UDP init
- I2C init
- ADS init
- Door init
- allgemeiner Status

---

## Testblock-spezifische Präfixe

### T15-2.1
Reine Aufheizreaktion

Verwendung für:
- Start Aufheizversuch
- feste Heater-Leistung
- erste Reaktion von Hotspot/Chamber
- Verzögerung zwischen Hotspot und Chamber

### T15-2.2
Nachlauf nach Heater OFF

Verwendung für:
- Heater OFF bei Chamber nahe Soll
- Beginn der Nachlaufmessung
- Peak-Erkennung
- Zeit bis Peak
- berechneter Overshoot

### T15-2.3
Overshoot-Karte / Abschaltpunkte

Verwendung für:
- definierte OFF-Punkte (z. B. Chamber 35 / 37 / 39 / 41 °C)
- Vergleich verschiedener Nachlaufkurven

### T15-3.1
Hotspot-Verhalten bei bekanntem tau

Verwendung für:
- Hotspot absolut
- Hotspot-Delta
- Beobachtung von problematischen Bereichen

### T15-3.2
Konservative Sicherheitsgrenzen

Verwendung für:
- Ableitung von hotspot_limit
- Ableitung von hotspot_delta_limit
- konservative Arbeitswerte

### T15-4.1
APPROACH-Tuning

Verwendung für:
- frühes Abbremsen
- predictive cutoff
- Verhalten nahe Zieltemperatur

### T15-4.2
HOLD-Tuning

Verwendung für:
- Fensterbetrieb
- Hold duty
- Stabilität im Zielband

### T15-5.0
Gesamtvalidierung

Verwendung für:
- Kaltstart
- Wiederanlauf
- Langzeithalten
- Door-open Tests
- unterschiedliche Zieltemperaturen

---

## Empfohlene Praxis

### 1. Pro Testlauf klaren Haupttag verwenden
Ein Lauf soll einen dominanten Tag haben.

Beispiel:
Wenn gerade tau bestimmt wird, dann sind die relevanten Logs primär:

- `[T15-2.2]`

### 2. BASE nur für allgemeine Infrastruktur
BASE nicht für alles verwenden.

Sonst wird Filterung unübersichtlich.

### 3. Relevante Ereignisse immer markieren
Wichtige Zeitpunkte sollen explizit geloggt werden:

- Heater ON
- Heater OFF
- Door change
- Peak reached
- Safety trip
- Controller mode change

### 4. Messdaten und Ereignisse trennen
Sinnvoll ist:
- periodische Messwerte
- separate Event-Logs

Beispiel:
- periodisch: Temperaturen, Duty, Mode
- Ereignis: „Heater OFF triggered“

---

## Beispielhafte Nutzung

### Basis
```cpp
T15_INFO(t15_log::BASE, "Boot complete\n");
```

### Test 2.2 – Nachlauf
```cpp
T15_INFO(t15_log::TEST_2_2, "Heater OFF at chamber=%.1fC hotspot=%.1fC\n", chamberC, hotspotC);
T15_INFO(t15_log::TEST_2_2, "Peak reached at chamber=%.1fC after %lus\n", peakC, secondsToPeak);
```

### Test 3.1 – Hotspot
```cpp
T15_WARN(t15_log::TEST_3_1, "Hotspot delta high: %.1fC\n", deltaC);
```

---

## Zusammenfassung

Die Logging-Klassifizierung dient dazu:

- empirische Tests sauber zu trennen
- einzelne Testblöcke gezielt zu filtern
- Ursachen und Wirkungen später nachvollziehen zu können

Die Grundregel lautet:

> Jede wichtige Logmeldung bekommt einen fachlich passenden T15-Testblock-Tag.
