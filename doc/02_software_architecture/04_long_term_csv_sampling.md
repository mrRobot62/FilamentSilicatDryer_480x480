# Long-Term CSV Sampling Design

## Ziel

Langzeit-Trocknungslaeufe sollen zusaetzlich zur bestehenden sekundaeren CSV-Ausgabe
verdichtete CSV-Daten fuer spaetere Stabilitaetsanalysen liefern.

Das Feature ist ausdruecklich eine **Ergaenzung**:

- bestehende `1x/s` CSV-Ausgaben bleiben unveraendert
- neue Langzeit-CSV wird zusaetzlich auf dem **HOST** erzeugt
- der CLIENT bleibt unveraendert und muss fuer die erste Ausbaustufe nicht neu geflasht werden

## Entscheidung

### Scope der ersten Ausbaustufe

- nur HOST-seitig
- nur fuer CSV-Ausgaben
- keine Aenderung an UART-Protokoll oder Client-Firmware
- Steuerung ueber den Parameter-Screen
- Persistenz ueber bestehende `HostParameters`-NVS-Struktur

### Begruendung

Der HOST besitzt bereits alle fuer die Langzeitbewertung relevanten Daten:

- Kammer-Temperatur
- Hotspot-Temperatur
- Sollwert
- Heater request / actual
- Door-Status
- Safety-Status
- Comm-Alive / LinkSync
- Material-/Heater-Stage

Diese Werte liegen bereits im `OvenRuntimeState` vor und werden zentral in
`src/app/oven/oven.cpp` verarbeitet und geloggt. Dadurch ist eine HOST-only
Aggregation technisch ausreichend und reduziert das Risiko.

## Fachliches Modell

### Bestehende CSVs

Unveraendert bleiben:

- `HOST_PLOT`
- `HOST_LOGIC`
- `CLIENT_PLOT`
- `CLIENT_LOGIC`

### Neue CSV

Neue zusaetzliche Zeile:

- `HOST_LONGRUN`

Die Zeile wird nicht sekundaer, sondern pro konfiguriertem Aggregationsfenster
ausgegeben.

## Aggregationsprinzip

### Erfassung

Intern sammelt der HOST weiterhin jede Sekunde einen Snapshot aus dem
aktuellen `OvenRuntimeState`.

### Ausgabefenster

Die Ausgabe erfolgt nach konfigurierbarem Fenster, z. B.:

- `10s`
- `30s`
- `60s`
- `300s`

Optional spaeter:

- `1s`
- `600s`
- `1800s`
- `3000s`

### Warum Aggregation statt einfachem Downsampling

Ein einzelner Samplepunkt alle `60s` kann kurze Peaks oder Schwingungen
vollstaendig verbergen. Fuer Temperaturstabilitaet ist deshalb `avg/min/max`
pro Fenster aussagekraeftiger als ein Einzelwert.

## Vorgeschlagenes Datenformat

### CSV Prefix

- `HOST_LONGRUN`

### Inhalt je Zeile

Pflichtfelder der ersten Version:

- Fensterlaenge in Sekunden
- Anzahl Samples im Fenster
- Solltemperatur `target_dC`
- Kammertemperatur `avg/min/max` in `dC`
- Hotspottemperatur `avg/min/max` in `dC`
- Heater request on-count
- Heater actual on-count
- Door-open seen
- Safety seen
- CommAlive loss seen
- LinkSync loss seen

### Beispiel

```text
[CSV_LR_HOST];60;60;825;823;818;829;901;892;915;14;12;0;0;0;0
```

Beispielhafte Feldreihenfolge:

```text
[CSV_LR_HOST];
window_s;
sample_count;
target_dC;
tch_avg_dC;tch_min_dC;tch_max_dC;
thot_avg_dC;thot_min_dC;thot_max_dC;
heater_req_on_count;
heater_actual_on_count;
door_open_seen;
safety_seen;
comm_loss_seen;
linksync_loss_seen
```

Hinweis:
Boolean-Flags als `seen` sind fuer Langzeitlaeufe robuster als ein einzelner
Momentanzustand bei Fensterschluss.

## Konfigurationsmodell

### Neue Host-Parameter

Erweiterung von `HostParameters` um:

- `uint8_t csvLongrunEnabled`
- `uint16_t csvLongrunIntervalSec`

Empfohlene Defaults:

- `csvLongrunEnabled = 0`
- `csvLongrunIntervalSec = 60`

### Validierung

Zulaessige Werte fuer `csvLongrunIntervalSec` in der ersten Version:

- `10`
- `30`
- `60`
- `300`

Optional spaeter erweiterbar.

### Versionierung

Die NVS-Struktur in `src/app/host_parameters.cpp` muss auf eine neue
Blob-Version angehoben werden. Alte gespeicherte Parameter fallen dann sauber
auf Defaults zurueck, wie bereits im aktuellen Mechanismus vorgesehen.

## UI-Design fuer den Parameterscreen

### Neue Gruppe

Neue Card im Parameterscreen:

- `CSV long-term`

### Felder

Mindestens:

- `Aktiv`
- `Intervall (s)`

### Bedienkonzept

Variante fuer erste Umsetzung:

- `Aktiv`: Stepper `0/1`
- `Intervall`: Stepper oder Roller mit festen Werten `10/30/60/300`

Praeferenz:

- `Aktiv` als `OFF/ON`-Roller
- `Intervall` als Roller mit festen Werten

Begruendung:
Das verhindert ungueltige Zwischenwerte und ist UI-seitig stabiler als ein
freier Stepper fuer diskrete Optionen.

## Technisches Design

### 1. CSV Schema erweitern

Datei:

- `include/log_csv.h`

Ergaenzen:

- neues CSV-Schema `HOST_LONGRUN`
- neues Makro `CSV_LOG_HOST_LONGRUN(...)`

### 2. Aggregator im Host

Datei:

- `src/app/oven/oven.cpp`

Neue interne Struktur, z. B.:

```cpp
struct HostLongrunAccumulator {
    uint32_t windowStartMs;
    uint16_t sampleCount;

    int64_t chamberSum_dC;
    int32_t chamberMin_dC;
    int32_t chamberMax_dC;

    int64_t hotspotSum_dC;
    int32_t hotspotMin_dC;
    int32_t hotspotMax_dC;

    int32_t target_dC;

    uint16_t heaterReqOnCount;
    uint16_t heaterActualOnCount;

    bool doorOpenSeen;
    bool safetySeen;
    bool commLossSeen;
    bool linkSyncLossSeen;
};
```

Benötigte Hilfsfunktionen:

- `longrun_reset(...)`
- `longrun_accumulate_sample(...)`
- `longrun_should_emit(...)`
- `longrun_emit_and_reset(...)`

### 3. Tick-/Poll-Anbindung

Der Aggregator soll dort laufen, wo der HOST-CSV-Kontext bereits zentral ist:

- in oder nahe `emit_csv_host_runtime_once_per_second(...)`
- auf Basis des aktuellen `runtimeState`

Empfehlung:

- bestehende `1x/s` Host-CSV unveraendert lassen
- im selben 1-Sekunden-Pfad den Longrun-Akkumulator fuettern
- nur bei aktivierter Option und vollem Fenster `HOST_LONGRUN` emittieren

Damit bleibt die Zeitbasis konsistent.

### 4. Parameter / Persistenz

Dateien:

- `include/host_parameters.h`
- `src/app/host_parameters.cpp`

Aenderungen:

- Struct erweitern
- Defaults setzen
- Validierung erweitern
- Blob-Version erhoehen

### 5. Parameterscreen

Datei:

- `src/app/ui/screens/screen_parameters.cpp`

Aenderungen:

- neue UI-Gruppe fuer CSV long-term
- Laden/Speichern ueber `s_edit_parameters`
- Dirty-State-Vergleich erweitern
- Save/Reset-Fluss bleibt unveraendert

## Verhalten bei Sonderfaellen

### Reboot waehrend Fenster

Teilfenster wird verworfen. Das ist fuer die erste Version akzeptabel.

### Deaktivierung waehrend Lauf

Beim Umschalten auf `OFF` wird der aktuelle Akkumulator verworfen und keine
Restzeile geschrieben.

### Intervallwechsel waehrend Lauf

Der aktuelle Akkumulator wird verworfen und mit neuem Fenster neu gestartet.

### Ungueltige Temperaturen

Falls spaeter wieder `TEMP_INVALID_DC` auftreten:

- Sample nicht in `avg/min/max` einrechnen
- `sample_count` nur fuer gueltige Werte fuehren

In der aktuellen Host-Runtime sind die Temperaturen bereits weitgehend
konsolidiert. Die Implementierung sollte trotzdem robust gegen ungueltige
Sentinel-Werte bleiben.

## Risiken

### Fachlich

- Zu grobes Intervall kann kurze Instabilitaeten verstecken
- Reines Downsampling waere irrefuehrend

### Technisch

- Erweiterung des NVS-Blobs invalidiert alte gespeicherte Parameter
- UI muss diskrete Intervallwerte sauber fuehren
- `avg/min/max` darf nicht versehentlich auf bereits gerundeten Strings,
  sondern auf numerischen `dC`-Werten berechnet werden

## Teststrategie

### Unit-/Funktionsnah

- Akkumulator mit kuenstlichen Samples fuettern
- korrektes `avg/min/max` pruefen
- korrektes Verhalten an Fenstergrenzen pruefen
- `enabled=0` pruefen
- Intervallwechsel pruefen

### Manuell

- CSV long-term `OFF`: keine `HOST_LONGRUN`-Zeilen
- CSV long-term `ON`, `10s`: Ausgabe alle 10 Sekunden
- `60s`: Ausgabe genau pro Minute
- Save/Reboot: Einstellungen bleiben erhalten
- Client bleibt ungeflasht und Gesamtsystem arbeitet unveraendert weiter

## Empfohlene Umsetzungsreihenfolge

1. CSV-Schema `HOST_LONGRUN` definieren
2. Host-Parameter erweitern und NVS-Version anheben
3. Akkumulator in `oven.cpp` implementieren
4. Parameterscreen um `CSV long-term` erweitern
5. manuelle Validierung mit `10s` und `60s`

## Ergebnis

Die erste sinnvolle Umsetzung ist eine HOST-only Langzeit-CSV mit
konfigurierbarem Intervall und aggregierten `avg/min/max`-Werten.

Das erfuellt das Ziel der Langzeitstabilitaetsanalyse, ohne die bestehende
sekundaere Diagnose-CSV oder die Client-Firmware anzutasten.
