
# T15 HeaterCurve Algorithm – Design & Test Parameters

## Ziel

Der HeaterCurve‑Algorithmus soll die **Chamber‑Temperatur stabil um eine Zieltemperatur halten**, ohne dass ein Overshoot entsteht.

Die Besonderheit des Systems:

- Heater reagiert **sehr schnell**
- Hotspot‑Sensor reagiert **schnell**
- Chamber‑Sensor reagiert **langsam**
- das System hat **thermische Trägheit**

Darum reicht eine einfache Thermostat‑ oder Hysterese‑Logik nicht aus.

Der Algorithmus berücksichtigt zusätzlich:

- Temperaturband
- Temperaturanstieg (Slope)
- Vorhersage zukünftiger Temperatur
- Hotspot‑Temperatur als Energieindikator

---

# Grundprinzip des HeaterCurve‑Algorithmus

## 1. Temperaturband statt Zielpunkt

Der Algorithmus regelt nicht auf exakt einen Wert.

Beispiel:

```
Target temperature        = 40°C
Allowed undershoot        = -10°C
Allowed overshoot         = +5°C
```

Erlaubter Bereich:

```
30°C ------------- 40°C ------------- 45°C
lower band         target            upper band
```

Logik:

| Zustand    | Verhalten               |
| ---------- | ----------------------- |
| unter 30°C | stark heizen            |
| 30–40°C    | moderat heizen          |
| 40–45°C    | vorsichtig / reduzieren |
| >45°C      | Heizung AUS             |

---

## 2. Chamber‑Slope (Temperaturanstieg)

Der Algorithmus misst, wie schnell sich die Chamber‑Temperatur verändert.

Berechnung:

```
slope = (current_temp - previous_temp) / dt
```

$slope= \frac{(current\_temp - previous\_temp)}{dt}$

Interpretation:

| slope | Bedeutung            |
| ----- | -------------------- |
| >0    | Chamber erwärmt sich |
| ≈0    | stabil               |
| <0    | Chamber kühlt ab     |

Wenn die Chamber bereits stark steigt, wird die Heizleistung reduziert.

---

## 3. Temperatur‑Vorhersage

Um Overshoot zu vermeiden, wird eine einfache Zukunftsabschätzung gemacht:

```
predicted_temp = current_temp + slope * tau
```

Parameter:

```
tau = thermische Nachlaufzeit
```

Beispiel:

```
current_temp = 38°C
slope        = 0.08 °C/s
tau          = 60 s

predicted_temp = 38 + 0.08 * 60
               = 42.8°C
```

Wenn die Vorhersage über dem oberen Band liegt, wird frühzeitig abgeschaltet.

---

## 4. Hotspot‑Überwachung

Der Hotspot reagiert deutlich schneller als die Chamber.

Er zeigt, wie viel Wärme aktuell im Heizbereich vorhanden ist.

Zwei wichtige Kriterien:

### Absolute Hotspot‑Grenze

Wenn der Hotspot eine maximale Temperatur überschreitet:

```
hotspot > hotspot_limit
```

→ Heizung sofort reduzieren oder ausschalten.

### Hotspot‑Delta

```
delta = hotspot - chamber
```

Wenn dieser Unterschied zu groß wird, bedeutet das:

- Heizbereich sehr heiß
- Chamber wird wahrscheinlich weiter steigen

→ Heizleistung reduzieren.

---

# Controller‑Modi

Der Algorithmus arbeitet mit klaren Zuständen.

## SAFE_OFF

Heater vollständig deaktiviert.

Auslöser:

- Door open
- Sensorfehler
- Übertemperatur
- SafetyLatch

---

## HEAT_UP

System deutlich unter Zielbereich.

Verhalten:

- hohe Heizleistung
- aber Hotspot‑Limit beachten

Ziel:

- schnelle Aufheizung

---

## APPROACH

Chamber nähert sich dem Zielbereich.

Verhalten:

- Heizleistung reduzieren
- Vorhersage prüfen
- Overshoot verhindern

---

## HOLD

Chamber innerhalb des Zielbands.

Verhalten:

- sanfte Heizleistung
- kleine Duty‑Zyklen
- stabile Temperaturhaltung

---

# Teststrategie zur Parameterbestimmung

T15 dient dazu, **alle relevanten Systemparameter experimentell zu ermitteln**.

Diese Parameter können nicht exakt berechnet werden, da sie stark von:

- Gehäuse
- Luftströmung
- Heizleistung
- Silicagel‑Masse
- Sensorposition

abhängen.

Darum müssen sie empirisch bestimmt werden.

---

# Wichtige Parameter für den Algorithmus

## 1. thermische Nachlaufzeit (tau)

Dieser Parameter bestimmt, wie stark der Temperaturanstieg in die Zukunft extrapoliert wird.

Messmethode:

1. Chamber aufheizen
2. Heater abrupt ausschalten
3. weiter Temperatur loggen

Beobachtung:

```
ΔT nach Abschalten
Zeit bis Maximum
```

Beispiel:

```
Heater OFF bei 40°C
Peak bei 44°C nach 70 s
```

→ tau ≈ 60–80 s

---

## 2. Chamber‑Slope‑Filter

Der rohe Temperaturanstieg ist oft verrauscht.

Darum wird ein Low‑Pass‑Filter benötigt.

Zu bestimmen:

```
slope_filter_alpha
```

Test:

- Log Rohwerte
- Log gefilterte slope
- Filter so einstellen, dass:
  - schnelle Änderungen sichtbar bleiben
  - Rauschen unterdrückt wird

---

## 3. Hotspot‑Limit

Der maximale erlaubte Hotspotwert.

Messung:

- System mehrfach aufheizen
- beobachten, bei welchem Hotspotwert Overshoot beginnt

Beispiel:

```
Hotspot > 95°C → Chamber overshoot
```

→ Limit eventuell:

```
hotspot_limit = 90°C
```

---

## 4. Hotspot‑Delta‑Limit

Differenz zwischen Hotspot und Chamber.

Messung:

Beobachten:

```
delta = hotspot - chamber
```

Wenn dieser Wert zu groß wird, ist viel Energie im Heizbereich gespeichert.

Typische Werte könnten sein:

```
delta_limit = 20–30°C
```

---

## 5. HOLD‑Duty

Wie viel Heizleistung nötig ist, um die Temperatur zu halten.

Messung:

1. System stabil im Zielbereich
2. Heaterleistung variieren
3. minimal nötige Leistung bestimmen

Beispiel:

```
Hold duty ≈ 15–25 %
```

---

## 6. APPROACH‑Reduktionsfaktor

Beim Nähern an das Ziel wird die Heizleistung reduziert.

Zu bestimmen:

```
approach_gain
```

Ziel:

- sanftes Abbremsen
- kein Überschwingen

---

# Test‑Logging (T15)

Für jeden Testlauf sollten folgende Werte geloggt werden:

```
timestamp
door_state
chamber_temp
hotspot_temp
heater_duty
controller_mode
chamber_slope
predicted_chamber_temp
safety_reason
```

Diese Logs ermöglichen später:

- Plotten der Heizkurve
- Analyse von Overshoot
- Parametertuning

---

# Ziel des T15 Tests

Am Ende der T15 Phase wollen wir:

1. stabile Heizkurve
2. minimales Overshoot
3. stabile HOLD‑Temperatur
4. reproduzierbares Verhalten

Die validierte Logik wird später in **T16 in den Host‑Controller (oven.cpp)** integriert.
