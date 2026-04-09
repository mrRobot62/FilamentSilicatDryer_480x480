# T16 Phase 4.0 Testkonzept

## Ziel

Dieses Testkonzept dient zur Verifikation von `T16_Phase_4.0`.

Scope von `T16_Phase_4.0`:
- Umbau von grober `FILAMENT` / `SILICA`-Unterscheidung
- hin zu preset-basierten `HeaterCurveProfile`
- ohne bereits die finale fachliche Feinabstimmung der Profile abzuschliessen

Ziel der Tests:
- Regressionen erkennen
- Preset-Zuordnung validieren
- Logging und Grundverhalten pruefen
- erste Plausibilitaet fuer `LOW_45C`, `MID_60C`, `HIGH_80C`, `SILICA_100C`

---

## Testumgebung

- Datum: 2026-03-31
- Firmware-Stand Host: T16_Phase_4.0
- Firmware-Stand Client: aktueller Stand, kein separater Fehler beobachtet
- Git-Commit: `2a40ae4`
- Tester: Bernhard Klein
- Raumtemperatur: je nach Lauf ca. 20-30 C, siehe jeweilige Logdatei
- Externes Thermometer: bei einzelnen Heiztests vorhanden
- Bemerkungen zur Hardware: PETG und ABS starteten mit erhoehter Chamber-Temperatur; Test 7 startete mit Chamber > 30 C

---

## Erwartete Logquellen

Bitte vor den Heiztests kurz pruefen:

- [x] `[CSV_CLIENT_PLOT]`
- [x] `[CSV_CLIENT_LOGIC]`
- [x] `[CSV_HOST_PLOT]`
- [x] `[CSV_HOST_LOGIC]`

Beobachtung:

```text
Alle vier erwarteten Logquellen vorhanden.
Logging-Regression bestanden.
Relevanter Log:
/Users/bernhardklein/Downloads/upd_log_viewer_logs/udp_log_20260331_214922.txt
```

---

## Test 1 - Preset-Mapping-Smoketest

### Ziel

Pruefen, dass Presets weiterhin korrekt ausgewaehlt werden und Zieltemperatur/Laufzeit plausibel uebernommen werden.

### Presets

- [ ] PLA
- [ ] PETG
- [ ] ABS
- [ ] SILICA

### Pruefpunkte

- [ ] Presetname korrekt
- [ ] Target korrekt
- [ ] Laufzeit korrekt
- [ ] keine UI-Auffaelligkeit
- [ ] keine Runtime-Auffaelligkeit

### Ergebnis

```text
PLA:
ok

PETG:
ok

ABS:
ok

SILICA:
ok
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

Bemerkungen:

```text
Preset-Mapping-Smoketest laut Testergebnis unauffaellig.
```

---

## Test 2 - Start/Stop-Regression

### Ziel

Pruefen, dass kurze Start/Stop-Laeufe fuer unterschiedliche Presets weiterhin korrekt funktionieren.

### Presets

- [ ] PLA
- [ ] PETG
- [ ] SILICA

### Pruefpunkte

- [ ] Start funktioniert
- [ ] Stop funktioniert
- [ ] keine unerwarteten Aktorzustaende
- [ ] Logs laufen stabil

### Ergebnis

```text
PLA:
ok

PETG:
ok

SILICA:
ok
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

Bemerkungen:

```text
Start/Stop-Regression laut Testergebnis ok.
```

---

## Test 3 - Logging-Regression

### Ziel

Pruefen, dass alle CSV-Logquellen weiterhin vorhanden und formatstabil sind.

### Pruefpunkte

- [ ] `[CSV_CLIENT_PLOT]` vorhanden
- [ ] `[CSV_CLIENT_LOGIC]` vorhanden
- [ ] `[CSV_HOST_PLOT]` vorhanden
- [ ] `[CSV_HOST_LOGIC]` vorhanden
- [ ] keine auffaelligen Aussetzer
- [ ] keine offensichtlichen Formatbrueche

### Ergebnis

```text
Alle vier CSV-Quellen im Log vorhanden.
Keine offensichtlichen Formatbrueche festgestellt.
```

### Relevante Logdatei

```text
/Users/bernhardklein/Downloads/upd_log_viewer_logs/udp_log_20260331_214922.txt
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

---

## Test 4 - LOW_45C Sanity-Test

### Ziel

Kurzer Plausibilitaetslauf fuer `LOW_45C`-Profil, z. B. mit PLA.

### Setup

- Preset:
- Preset: PLA
- Target: 47.0 C
- Laufzeit: ca. 6-8 min, ohne Cooldown
- Door bleibt geschlossen: ja

### Pruefpunkte

- [ ] Lauf startet sauber
- [ ] Temperaturanstieg plausibel
- [ ] keine grobe Regression
- [ ] keine unerwartete Safety

### Beobachtung

```text
Start Chamber 22.7 C, Peak Chamber 52.2 C, Laufende bei 47.0 C.
Heater-Pulse plausibel: 10.1 s, 7.1 s, danach mehrfach ca. 6 s.
LOW_45C Profilpfad wirkt unveraendert plausibel.
```

### Logdatei

```text
/Users/bernhardklein/Downloads/fsd_phase40/fsd_phase40a_udp_log_20260331_220149.txt
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

---

## Test 5 - MID_60C Sanity-Test

### Ziel

Kurzer Plausibilitaetslauf fuer `MID_60C`-Profil, z. B. mit PETG.

### Setup

- Preset:
- Preset: PETG
- Target: 62.0 C
- Laufzeit: ca. 6-8 min, ohne Cooldown
- Door bleibt geschlossen: ja

### Pruefpunkte

- [ ] Lauf startet sauber
- [ ] Temperaturanstieg plausibel
- [ ] keine grobe Regression
- [ ] keine unerwartete Safety

### Beobachtung

```text
Start Chamber 29.0 C, Peak Chamber 64.2 C, Ende 58.8 C.
Heater-Pulse plausibel: 10.1 s, 10.2 s, 7.1 s, danach mehrfach ca. 6.1 s.
MID_60C Profilpfad plausibel, kein grober Regressionseffekt.
```

### Logdatei

```text
/Users/bernhardklein/Downloads/fsd_phase40b/fsd_phase40b_udp_log_20260331_221330.txt
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

---

## Test 6 - WAIT / Door-Regression

### Ziel

Pruefen, dass der vorhandene WAIT-/Door-Pfad weiterhin funktioniert.

### Setup

- Preset:
- Preset: nicht separat dokumentiert
- Target: nicht separat dokumentiert
- Door-Open-Dauer: nicht separat dokumentiert
- Zeitpunkt manuelles Fortsetzen: nicht separat dokumentiert

### Pruefpunkte

- [ ] Wechsel nach WAIT korrekt
- [ ] Heater geht aus
- [ ] Door-Status im Log sichtbar
- [ ] manuelles Fortsetzen funktioniert
- [ ] keine offensichtliche Regression im Resume-Verhalten

### Beobachtung

```text
Testergebnis: ok.
Kein separates Log bereitgestellt.
```

### Logdatei

```text
kein separates Log
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

---

## Test 7 - HIGH_80C Sanity-Test

### Ziel

Kurzer Plausibilitaetslauf fuer `HIGH_80C`-Profil, z. B. mit ABS oder aehnlichem High-Temp-Filament.

### Setup

- Preset:
- Preset: ABS
- Target: 82.0 C
- Laufzeit: ca. 6-8 min, ohne Cooldown
- Door bleibt geschlossen: ja

### Pruefpunkte

- [ ] Lauf startet sauber
- [ ] Temperaturanstieg plausibel
- [ ] keine grobe Regression
- [ ] keine unerwartete Safety

### Beobachtung

```text
Start Chamber 30.5 C, Peak Chamber 78.1 C.
Kurzer Sanity-Test ohne Zielerreichung ueber volle Haltezeit.
Keine grobe Regression sichtbar.
HIGH_80C Profilpfad fuer kurzen Test plausibel.
```

### Logdatei

```text
/Users/bernhardklein/Downloads/fsd_phase40c/fsd_phase40c_udp_log_20260331_222945.txt
```

### Bewertung

- [x] bestanden
- [ ] auffaellig

---

## Test 8 - SILICA_100C Smoketest

### Ziel

Nur grundlegender Profil-/Pfadtest fuer `SILICA_100C`, noch keine finale fachliche Bewertung.

### Setup

- Preset:
- Preset: SILICA
- Target: 100.0 C
- Laufzeit: nur kurz angelaufen
- Externes Thermometer vorhanden: nicht separat dokumentiert

### Pruefpunkte

- [ ] Presetpfad funktioniert
- [ ] Lauf startet sauber
- [ ] Logging vorhanden
- [ ] keine offensichtliche Fehlfunktion

### Beobachtung

```text
Presetpfad und Logging funktionieren.
Fachlich aber klar noch nicht akzeptabel:
Start Chamber 37.8 C, Peak Chamber 134.1 C, Peak Hotspot 118.8 C.
Erste Heater-Phase wieder extrem lang mit ca. 102.9 s.
Dieser Test bestaetigt den offenen Handlungsbedarf fuer SILICA_100C.
```

### Logdatei

```text
/Users/bernhardklein/Downloads/fsd_phase40_7/fsd_phase40_7_udp_log_20260331_223635.txt
```

### Bewertung

- [ ] bestanden
- [x] auffaellig

---

## Gesamtauswertung

### Zusammenfassung

```text
T16_Phase_4.0 ist als Architektur- und Regressionstest insgesamt bestanden.
Preset-Mapping funktioniert.
Logging funktioniert.
PLA, PETG und ABS zeigen keine grobe Regression.
SILICA_100C bleibt fachlich offen und ist der naechste Hauptarbeitspunkt.
```

### Kritische Befunde

```text
SILICA_100C:
- deutlicher Overshoot
- erste Heater-Phase weiterhin viel zu lang
- High-Temp-Regelung fachlich noch nicht geeignet
```

### Nicht-kritische Befunde

```text
PLA:
- leichter Overshoot im kurzen Lauf, aber fuer Regressionstest noch akzeptabel

PETG:
- plausibles Verhalten, leichter Overshoot

ABS:
- kurzer Lauf plausibel, aber noch kein echtes fachliches Tuningurteil
```

### Empfehlung fuer naechsten Schritt

- [ ] Phase 4.0 als bestanden ansehen
- [ ] Phase 4.1 fuer fachliches Profiltuning beginnen
- [ ] zuerst Logging-/Kalibrierproblem untersuchen
- [ ] zuerst Heizkurvenproblem untersuchen
- [x] Phase 4.0 als bestanden ansehen
- [x] Phase 4.1 fuer fachliches Profiltuning beginnen
- [ ] zuerst Logging-/Kalibrierproblem untersuchen
- [x] zuerst Heizkurvenproblem untersuchen

Begruendung:

```text
Die Profil-Matrix funktioniert als Architekturgrundlage.
Der naechste sinnvolle Schritt ist die fachliche Differenzierung der Profile.
Prioritaet hat SILICA_100C mit bounded pulses und chamber-dominierter High-Temp-Strategie.
Eine NTC-Kalibrierung bleibt relevant, ist aber nicht der erste Blocker fuer Phase 4.1.
```
