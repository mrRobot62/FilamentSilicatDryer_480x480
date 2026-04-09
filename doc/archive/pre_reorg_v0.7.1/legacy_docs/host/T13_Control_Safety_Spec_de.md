# T13 Steuerungs- und Sicherheits-Spezifikation

**Projekt:** FilamentSilicatDryer_480x480\
**Architektur-Version:** T13 (Dual-NTC, relaisfreundliche
Pulssteuerung)\
**Datum:** 2026-02-19

------------------------------------------------------------------------

# Änderungsprotokoll

## T13 (2026-02-19)

-   Einführung der Dual-NTC Architektur:
    -   `tempHotspot` (Sicherheitsüberwachung und thermische Dynamik)
    -   `tempChamber` (Primäre Regel- und UI-Temperatur)
-   Vollständige Entfernung des PT1/Bias-Thermalmodells.
-   Einführung preset-basierter Kammergrenzen (`chamberMaxC`).
-   Einführung eines relaisfreundlichen adaptiven Pulsreglers.
-   Einführung eines mehrstufigen Sicherheitssystems:
    -   Preset-basierte Kammergrenze
    -   Absolute Kammergrenze
    -   Hotspot Soft- und Hard-Grenzen
    -   Fail-Safe Sensorbehandlung

------------------------------------------------------------------------

# Systemübersicht

Das System besteht aus:

-   Host: ESP32-S3 (UI, Regelungslogik, Sicherheitslogik)
-   Client: ESP32-WROOM (Sensorerfassung, Aktorsteuerung)
-   Temperatursensoren: Zwei unabhängige NTC-Sensoren über ADS1115 ADC

Temperaturrollen:

  Sensor        Zweck
  ------------- ------------------------------------------------------
  tempChamber   Primäre Regeltemperatur
  tempHotspot   Sicherheitsüberwachung und dynamische Heizbegrenzung

Der Host ist die **Single Source of Truth** für alle Regel- und
Sicherheitsentscheidungen.

------------------------------------------------------------------------

# Sensorerfassung (Client)

ADC: ADS1115\
Gain: ±6.144 V\
Auflösung: 16-bit signed integer

Mapping:

  ADS Kanal   Zweck
  ----------- -------------
  CH0         tempHotspot
  CH1         tempChamber
  CH2         Reserviert
  CH3         Reserviert

RAW-Werte werden unverändert übertragen.

Temperaturübertragungseinheit:

-   deci-Grad Celsius (°C × 10)
-   Beispiel: 509 = 50.9°C

------------------------------------------------------------------------

# STATUS Protokollframe

Format:

    C;STATUS;<mask_hex>;<adc0>;<adc1>;<adc2>;<adc3>;<tempHotspot_dC>;<tempChamber_dC>

Beispiel:

    C;STATUS;0035;12345;11890;0;0;509;372

------------------------------------------------------------------------

# Preset-Modell

Jedes Preset definiert:

  Feld          Beschreibung
  ------------- -------------------------------------
  targetC       Zieltemperatur
  toleranceC    Hysterese
  chamberMaxC   Maximale zulässige Kammertemperatur

Beispiel (Silicagel Preset):

  Feld          Wert
  ------------- -------
  targetC       105.0
  toleranceC    2.0
  chamberMaxC   110.0

Regelband:

    lo = targetC − toleranceC
    hi = targetC + toleranceC

Beispiel:

    103°C … 107°C

------------------------------------------------------------------------

# Sicherheitsmodell

Sicherheitsebenen (höchste Priorität zuerst):

## Ebene 1 --- Absolute Kammergrenze

    CHAMBER_HARD_MAX_C = 120°C

Bei Überschreitung:

    Heater sofort AUS

## Ebene 2 --- Preset-Kammergrenze

    capC = min(chamberMaxC, CHAMBER_HARD_MAX_C)

Bei Überschreitung:

    Heater AUS

## Ebene 3 --- Hotspot Sicherheit

Relative Grenzwerte:

    HOTSPOT_SOFT_MAX_C = capC + 15°C
    HOTSPOT_HARD_MAX_C = capC + 30°C

Verhalten:

  Bedingung       Aktion
  --------------- ---------------------------
  ≥ Soft-Grenze   Verlängerung der OFF-Zeit
  ≥ Hard-Grenze   Heater AUS

Globale Begrenzung:

    HOTSPOT_HARD_MAX_C ≤ 200°C

------------------------------------------------------------------------

# Pulsregler (relaisfreundlich)

Relais benötigen langsame Schaltzyklen.

Standardparameter:

    HEAT_MS_MIN = 2000
    HEAT_MS_MAX = 8000

    REST_MS_MIN = 6000
    REST_MS_MAX = 60000

    MIN_TOGGLE_MS = 2000

Verhalten nahe Zieltemperatur:

    nearBandC = 3.0°C
    restNearTargetMinMs = 20000

Verhalten:

  Bedingung                    Ergebnis
  ---------------------------- ------------------------------------
  Weit unter Ziel              Moderate Heizpulse
  Nahe Ziel                    Minimale Heizzeit, lange OFF-Phase
  Schneller Hotspot-Anstieg    Verlängerte OFF-Phase
  Sicherheitsgrenze erreicht   Heater AUS

------------------------------------------------------------------------

# Fail-Safe Verhalten

Ungültiger Temperaturwert:

    temp = -32768

Verhalten:

    Heater AUS
    Optional Fault-Status

------------------------------------------------------------------------

# Regelautorität

Regelentscheidungen basieren ausschließlich auf:

    tempChamberC

Hotspot wird nur verwendet für:

-   Sicherheit
-   Timing-Anpassung

UI zeigt:

    tempChamberC

------------------------------------------------------------------------

# Beispiel: Silicagel Betrieb

Preset:

    target = 105°C
    tolerance = 2°C
    max = 110°C

Stabiler Betriebsbereich:

    100°C … 110°C

Overshoot wird verhindert durch:

-   chamberMaxC
-   Pulsregler
-   Hotspot-Sicherheitslogik

------------------------------------------------------------------------

# Implementierungsanforderungen

Host muss:

-   STATUS Frame parsen
-   Temperaturen verwalten
-   Pulsregler ausführen
-   Sicherheitslogik anwenden
-   Heater über Bitmaske steuern

Client muss:

-   ADS1115 lesen
-   NTC Temperaturen berechnen
-   STATUS Frames senden

------------------------------------------------------------------------

# Ende der Spezifikation
