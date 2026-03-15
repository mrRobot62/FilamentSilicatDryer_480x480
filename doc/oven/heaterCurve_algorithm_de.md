
# T15 HeaterCurve Algorithmus – Beschreibung

## Zweck

Dieses Dokument beschreibt das aktuelle **HeaterCurve‑Regelkonzept** für den T15‑Test‑Endpoint.

Ziel ist es, eine Regelstrategie zu entwickeln, die später in **T16 / host‑seitige Ofensteuerung (oven.cpp)** integriert werden kann und die **Chamber‑Temperatur stabil um eine Zieltemperatur hält**, ohne dass ein Overshoot entsteht.

Das gewünschte Verhalten:

- Chamber‑Temperatur um `SIM_TARGET_TEMP` halten
- maximal erlaubtes Überschwingen: `SIM_TARGET_MAX_OVERSHOT`
- maximal erlaubtes Unterschwingen: `SIM_TARGET_MIN_UNDERSHOT`
- Sicherheit hat immer Vorrang vor Regelperformance

---

# Systemkontext

T15 ist eine **Client‑seitige Laborumgebung**.

Es ist **nicht die finale Architektur**.

Die finale Architektur bleibt:

- **Host / oven.cpp** = Single Source of Truth
- **Client** = Aktuatorsteuerung + Telemetrie + Safety

T15 dient ausschließlich dazu, einen stabilen Heizalgorithmus mit realer Hardware zu entwickeln.

---

# Verfügbare Eingangsgrößen

## 1. Chamber Temperatur
Quelle: ADS1115 Kanal A1  
NTC Typ: 10k

Bedeutung:

Dies ist die **primäre Regelgröße**.  
Die Chamber‑Temperatur ist die Temperatur, die später stabil gehalten werden soll.

---

## 2. Hotspot Temperatur
Quelle: ADS1115 Kanal A0  
NTC Typ: 100k

Bedeutung:

Der Hotspot ist ein **schnell reagierender Indikator für eingebrachte Heizenergie**.

Er ist **nicht die primäre Regelgröße**, hilft aber dabei Overshoot früh zu erkennen.

---

## 3. Door Zustand

Definition:

```
DOOR_OPEN  = HIGH (5V)
DOOR_CLOSED = LOW (GND)
```

Dies ist ein **harte Sicherheitsbedingung**:

Door open → Heater AUS  
Door closed → Heater darf arbeiten

---

## 4. Zeit

Zeit basiert auf `millis()`.

Sie wird benötigt für:

- Abtastung
- Regelintervall
- Temperaturanstieg (Slope)
- Fenstersteuerung

---

# Ausgangsgröße des Controllers

Der Controller erzeugt eine Heizanforderung in Prozent:

```
0%   = Heater AUS
100% = maximale Heizleistung
```

Der Client setzt dies auf reale Hardware um:

- PWM auf Heater‑Pin
- 4 kHz
- robustes Attach / Detach
- definierter Safe‑Level

---

# Sicherheitsphilosophie

Sicherheit hat Vorrang vor Regelverhalten.

Der Heater muss abgeschaltet werden bei:

1. Door open
2. Chamber Sensor ungültig
3. Hotspot zu hoch
4. Chamber über maximaler Temperatur
5. zukünftigen Sicherheitsbedingungen

---

# Safety‑Latch Konzept

T15 verwendet ein **Latch‑Verhalten**.

Wenn eine kritische Bedingung eintritt:

- Heater wird abgeschaltet
- System bleibt im sicheren Zustand
- Reset nur durch Neustart

Dieses Verhalten entspricht der Sicherheitsstrategie von T14.

---

# Ursache für Overshooting

Overshoot entsteht durch thermische Trägheit.

Ablauf:

1. Heater bringt Energie sehr schnell ein
2. Hotspot reagiert schnell
3. Chamber reagiert langsam
4. Heater wird abgeschaltet
5. gespeicherte Wärme lässt Chamber weiter steigen

Eine einfache Hysterese schaltet deshalb zu spät ab.

---

# Strategie zur Vermeidung von Overshoot

Der HeaterCurve‑Algorithmus verwendet drei Mechanismen:

## 1. Zielband statt Punkt

Der Zielwert wird als Band betrachtet.

```
lower = target - minUndershoot
upper = target + maxOvershoot
```

Verhalten:

| Bereich | Verhalten |
|-------|-----------|
| unterhalb Band | stärker heizen |
| innerhalb Band | vorsichtig regeln |
| oberhalb Band | Heizung AUS |

---

## 2. Chamber‑Steigung

Der Controller beobachtet, wie schnell sich die Temperatur verändert.

Berechnung:

```
slope = (currentTemp - previousTemp) / dt
```

Interpretation:

positive slope → Chamber wird wärmer  
negative slope → Chamber kühlt ab

Wenn die Chamber bereits stark steigt, wird Heizleistung reduziert.

---

## 3. Temperaturvorhersage

Der Algorithmus berechnet eine einfache Prognose:

```
predicted = currentTemp + slope * tau
```

Parameter:

```
tau = thermische Nachlaufzeit
```

Wenn die vorhergesagte Temperatur über dem oberen Band liegt, wird der Heater frühzeitig abgeschaltet.

Dies ist der wichtigste Anti‑Overshoot‑Mechanismus.

---

# Rolle des Hotspot Sensors

Der Hotspot reagiert schneller als die Chamber.

Er zeigt:

- wie viel Energie aktuell im Heizbereich vorhanden ist

Wenn der Hotspot sehr hoch ist oder die Differenz groß wird:

```
delta = hotspot - chamber
```

bedeutet dies:

- Heizbereich sehr heiß
- Chamber wird später nachziehen

→ Heizleistung reduzieren oder stoppen.

---

# Controller‑Modi

## SAFE_OFF

Heater komplett deaktiviert.

Auslöser:

- Door open
- Sensorfehler
- Sicherheitsbedingung

---

## HEAT_UP

System ist deutlich unter Zieltemperatur.

Verhalten:

- hohe Heizleistung
- Hotspot‑Limit beachten

Ziel:

- schnelle Aufheizung

---

## APPROACH

Chamber nähert sich dem Zielbereich.

Verhalten:

- Leistung reduzieren
- Vorhersage prüfen
- Overshoot vermeiden

---

## HOLD

Temperatur liegt im Zielband.

Verhalten:

- geringe Heizleistung
- sanfte Energiezufuhr
- stabile Temperaturhaltung

---

# Warum Fenster‑Duty im HOLD verwendet wird

Statt permanent Leistung zu geben wird ein Zeitfenster verwendet.

Beispiel:

```
Fenster = 2000 ms
Duty = 25%
```

Heater läuft nur einen Teil des Fensters.

Vorteile:

- weniger Temperaturschwingungen
- besser beobachtbar
- stabileres Verhalten

---

# Evaluierung der HeaterCurve

Der Algorithmus wird über reale Tests validiert.

Zu beobachten:

- Door Zustand
- Chamber Temperatur
- Hotspot Temperatur
- Heater Duty
- Controller Mode
- Chamber Slope
- Vorhersage Temperatur
- Safety Flags

---

# Erfolgsdefinition

Der Algorithmus gilt als erfolgreich wenn:

1. Chamber Zieltemperatur erreicht
2. Überschwingen minimal bleibt
3. Temperatur lange stabil bleibt
4. Hotspot nicht kritisch wird
5. Door immer sofort Heater OFF erzwingt
6. Verhalten aus Logs nachvollziehbar ist

---

# Warum das besser als Hysterese ist

Eine einfache Hysterese berücksichtigt nur den aktuellen Wert.

Der HeaterCurve‑Algorithmus berücksichtigt zusätzlich:

- Zielband
- Temperaturanstieg
- Vorhersage
- Hotspot‑Differenz
- Sicherheitsbedingungen

Dadurch reagiert er nicht nur auf die aktuelle Temperatur, sondern auf die **zukünftige Entwicklung**.

---

# Zusammenfassung

Die HeaterCurve Strategie:

- schnell heizen wenn kalt
- Leistung reduzieren bevor Ziel erreicht wird
- früh abschalten wenn Überschwingen droht
- sanft halten im Zielbereich
- Sicherheit hat immer Priorität
