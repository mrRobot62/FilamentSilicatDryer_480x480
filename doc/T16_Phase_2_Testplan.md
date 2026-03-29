# T16 Phase 2 Testplan

## Ziel

Die neue Host-Heizstrategie soll unter realen Bedingungen beobachtet und gegen die fachlichen Risiken abgesichert werden:

- Overshoot bei Filament minimieren
- Silica robust und zuegig aufheizen
- Safety-Regeln jederzeit wirksam halten
- Stages und Material-Policy im Log sichtbar machen

## Automatisiert sinnvoll

### Build-Validierung

- `pio run -e host_esp32s3_st7701`
- `pio run -e client_esp32_wroom`

### Spaeter sinnvoll zu ergaenzen

- reine Host-Tests fuer:
  - Stage-Uebergang `BULK_HEAT -> APPROACH -> HOLD`
  - Policy-Auswahl `FILAMENT` vs. `SILICA`
  - Safety-Cutoff bei Door / Chamber / Hotspot / Overshoot

## Manuelle Hardwaretests

### Test 1: Filament, kalter Start

- Preset `PLA` oder `TPU` waehlen
- Ofen im kalten Zustand starten
- beobachten:
  - Stage startet in `BULK_HEAT`
  - spaeter Wechsel nach `APPROACH`
  - nahe Soll Wechsel nach `HOLD`
  - kein unruhiges Relais-Klappern

### Test 2: Filament nahe Soll

- Ofen bereits vorgewaermt
- Filament-Preset mit niedrigerem Target waehlen
- beobachten:
  - Heizer startet nicht blind aggressiv
  - Regelung bleibt vorsichtig
  - kein grober Overshoot ueber Soll

### Test 3: Silica

- Preset `SILICA` waehlen
- Lauf bis in den Zielbereich
- beobachten:
  - groessere Heizfenster als bei Filament
  - trotzdem Safety wirksam
  - Stage-Wechsel sichtbar

### Test 4: Door Safety

- waehrend `RUNNING` Tuer oeffnen
- erwarten:
  - Heater Request faellt ab
  - Safety aktiv
  - Door im Log sichtbar

### Test 5: Communication / Sync

- Host/Client kurzzeitig stoeren oder neu starten
- erwarten:
  - `commAlive` / `linkSynced` reagieren sichtbar
  - kein unkontrolliertes Heizen

## Relevante Logfelder

### HOST_PLOT

- chamber
- hotspot
- target
- low
- high
- safety

### HOST_LOGIC

- mode
- running
- heater_request_on
- heater_actual_on
- door
- safety
- commAlive
- linkSynced
- materialClass
- heaterStage

## Abnahmekriterien fuer die naechste Iteration

- Filament zeigt keinen groben, laenger anstehenden Overshoot
- Stage-Uebergaenge sind im Log plausibel
- Silica verwendet die getrennte Policy sichtbar
- Safety schaltet reproduzierbar ab
- Relais bleibt schalttechnisch ruhig

## Testhinweis

Ab diesem Phasenstand sind echte Hardware-Runtimetests nicht nur sinnvoll, sondern fuer weiteres Heizkurven-Tuning notwendig. Reine Builds reichen ab hier nicht mehr aus, um die fachliche Qualitaet der Heizkurve zu bewerten.
