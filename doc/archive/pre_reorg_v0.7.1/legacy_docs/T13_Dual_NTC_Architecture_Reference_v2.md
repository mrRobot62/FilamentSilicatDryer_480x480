# T13 -- Dual‑NTC Architecture Reference (Erweitert)

Version: T13.0\
Status: Stabil, Referenzbasis für T14 SafetyGuard\
Zielgruppe: Entwickler, Techniker, Elektronik‑Interessierte

------------------------------------------------------------------------

# Übersicht

Die T13‑Architektur implementiert ein temperaturgeregeltes
Filament‑Trocknungssystem mit zwei physikalischen Temperatursensoren
(NTCs).\
Die Regelung erfolgt vollständig auf dem Host‑Controller (ESP32‑S3).

Die Architektur trennt bewusst:

-   Temperaturregelung (Control)
-   Sicherheit (Safety)
-   Hardwarezugriff (Client)
-   Darstellung (UI)

------------------------------------------------------------------------

# Sensor‑Architektur

## Chamber‑NTC (Control Sensor)

Eigenschaften:

-   10k NTC
-   10k Referenzwiderstand
-   angeschlossen an ADS1115 Channel 1
-   misst reale Lufttemperatur im Trocknungsraum

Verwendung:

-   einzige Grundlage der Temperaturregelung
-   einzige Grundlage für UI‑Anzeige
-   einzige Grundlage für Heater‑Steuerung

------------------------------------------------------------------------

## Hotspot‑NTC (Safety Sensor)

Eigenschaften:

-   100k NTC
-   100k Referenzwiderstand
-   angeschlossen an ADS1115 Channel 0
-   misst Nähe zur Heizquelle

Verwendung:

-   ausschließlich Sicherheitsüberwachung
-   keine Verwendung für Regelung

------------------------------------------------------------------------

# Systemarchitektur

``` mermaid
flowchart LR
    ChamberNTC --> ADS1115
    HotspotNTC --> ADS1115

    ADS1115 --> ClientESP32
    ClientESP32 -->|STATUS Frame| HostESP32

    HostESP32 --> OvenControl
    OvenControl --> HeaterControl

    OvenControl --> UI
```

------------------------------------------------------------------------

# Datenfluss im Betrieb

``` mermaid
sequenceDiagram
    participant ChamberNTC
    participant ADS1115
    participant Client
    participant Host
    participant OvenControl

    ChamberNTC->>ADS1115: Analogspannung
    ADS1115->>Client: ADC Raw Value
    Client->>Host: STATUS Frame
    Host->>OvenControl: Update runtimeState.tempChamberC
    OvenControl->>HeaterControl: ON / OFF
```

------------------------------------------------------------------------

# STATUS‑Protokoll (T13)

Frame Format:

    C;STATUS;<mask>;<adc0>;<adc1>;<adc2>;<adc3>;<tempHotspot_dC>;<tempChamber_dC>

Beispiel:

    C;STATUS;0020;26833;14065;0;0;-32768;227

Bedeutung:

-   adc1 = Chamber Sensor
-   tempChamber_dC = Chamber Temperatur in 0.1°C

------------------------------------------------------------------------

# Regelarchitektur (Control Layer)

Regelung basiert ausschließlich auf:

    tempChamberC

Nicht verwendet:

    tempHotspotC

------------------------------------------------------------------------

# Regel‑State‑Machine

``` mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Heating : temp < target‑hysterese
    Heating --> Stabilizing : temp nahe target
    Stabilizing --> Heating : temp fällt
    Stabilizing --> Idle : target erreicht
```

------------------------------------------------------------------------

# Technische Implementierung

## Übersicht der Software‑Komponenten

``` mermaid
flowchart TD

    subgraph Client (ESP32‑WROOM)
        ADS[ADS1115 Driver]
        NTCConv[NTC Conversion]
        StatusGen[ProtocolStatus Generator]
    end

    subgraph Host (ESP32‑S3)
        HostComm[HostComm RX]
        ProtocolParse[Protocol Parser]
        RuntimeState[OvenRuntimeState]
        ControlLoop[Control Loop]
        Heater[Heater Output]
    end

    ADS --> NTCConv
    NTCConv --> StatusGen
    StatusGen --> HostComm

    HostComm --> ProtocolParse
    ProtocolParse --> RuntimeState
    RuntimeState --> ControlLoop
    ControlLoop --> Heater
```

------------------------------------------------------------------------

## Client‑Implementierung

Dateien:

-   FSD_Client.cpp
-   ClientComm.cpp

Aufgaben:

-   ADS1115 auslesen
-   ADC Werte in Temperatur konvertieren
-   STATUS Frame erzeugen
-   STATUS über UART senden

Beispielablauf:

``` cpp
adcRaw[1] = ads.readADC_SingleEnded(1);
tempChamber_dC = ntc_convert(adcRaw[1]);
sendStatusFrame();
```

------------------------------------------------------------------------

## Host‑Implementierung

Dateien:

-   oven.cpp
-   HostComm.cpp

Ablauf:

1.  STATUS Frame empfangen
2.  Werte in runtimeState übernehmen
3.  Regelalgorithmus ausführen
4.  Heater setzen

``` mermaid
flowchart TD
    STATUS[STATUS Frame] --> Parse
    Parse --> runtimeState
    runtimeState --> ControlLogic
    ControlLogic --> HeaterOutput
```

------------------------------------------------------------------------

## OvenRuntimeState

Zentrale Datenstruktur:

Beispiel:

    struct OvenRuntimeState
    {
        float tempChamberC;
        float tempHotspotC;
        float tempTargetC;

        bool heaterOn;
    };

Single Source of Truth für:

-   Regelung
-   UI
-   Logging

------------------------------------------------------------------------

## Regelalgorithmus (T13 Control Core)

Prinzip:

-   Thermostat‑basierte Regelung
-   Hysterese
-   overshoot prevention

Beispiel:

    if tempChamberC < target‑hysterese → heater ON
    if tempChamberC > target → heater OFF

------------------------------------------------------------------------

# Sicherheitsarchitektur

Safety erfolgt über:

-   Host‑Safety Logic
-   (zukünftig erweitert in T14 SafetyGuard Client)

------------------------------------------------------------------------

# Ergebnis der T13 Architektur

Eigenschaften:

-   stabile Regelung
-   keine Overshoot‑Instabilität
-   klare Trennung von Control und Safety
-   deterministic behaviour

------------------------------------------------------------------------

# Bedeutung für T14

T13 dient als stabile Grundlage für:

-   SafetyGuard Client Layer
-   Hardware Fail‑Safe Mechanismen
