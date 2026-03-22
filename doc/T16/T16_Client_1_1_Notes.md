# T16.Client.1.1 – Heater-I/O aus FSD_Client.cpp auslagern

## Ziel
- Heater-PWM physisch in ein eigenes Client-Modul verschieben
- `FSD_Client.cpp` behält nur noch die Entscheidung **ein/aus**
- kleiner Door-Init-Cleanup gleich mit erledigen

## Dateien aus diesem ZIP
- `include/client/heater_io.h`
- `src/client/heater_io.cpp`

## Zusätzliche Änderungen in `src/client/FSD_Client.cpp`

### 1) Include ergänzen
Direkt nach:
```cpp
#include "client/sensor_ntc.h"
```
ergänzen:
```cpp
#include "client/heater_io.h"
```

### 2) `isDoorOpen()` minimal vereinheitlichen
Falls noch nicht geschehen, verwende:
```cpp
static bool isDoorOpen() {
    const bool now = sensor_ntc::is_door_open();

    static bool last = false;
    if (now != last) {
        CLIENT_INFO("[DOOR] state=%s\n", now ? "OPEN" : "CLOSED");
        last = now;
    }
    return now;
}
```

### 3) Wrapper `heaterPwmEnable(...)` ersetzen
Die komplette Funktion:
```cpp
static uint32_t heaterDutyFromPercent(...)
...
static void heaterPwmEnable(bool enable) { ... }
```
komplett entfernen und stattdessen nur diesen Wrapper verwenden:
```cpp
static void heaterPwmEnable(bool enable) {
    (void)heater_io::set_enabled(enable, heater_io::kDefaultDutyPercent);
}
```

### 4) Alle direkten Zugriffe auf `g_heaterPwmRunning` ersetzen
Ersetze im gesamten `FSD_Client.cpp`:

```cpp
g_heaterPwmRunning
```

durch:

```cpp
heater_io::is_running()
```

Betroffene Stellen sind typischerweise:
- in `applyOutputs(...)`
- in `clientComm.hasNewOutputsMask()`
- im Door-Transition-Watcher

### 5) Heater-Safe-Init in `setup()`
Nach der Output-Pin-Initialisierung ergänzen:
```cpp
heater_io::init_off();
```

Das ist bewusst okay, auch wenn GPIO12 schon als normaler Output initialisiert wurde.
So wird der Heater-Pfad explizit in einen sicheren OFF-Zustand gesetzt.

### 6) Doppelten Door-Init-Log entfernen
Den alten manuellen Block am Ende von `setup()` löschen:

```cpp
const bool door_open = (digitalRead(OVEN_DOOR_SENSOR) != 0);
CLIENT_INFO("[IO] DOOR init done: GPIO=%d INPUT_PULLUP level=%d (%s)\n",
            OVEN_DOOR_SENSOR,
            door_open ? 1 : 0,
            door_open ? "OPEN" : "CLOSED");
```

Denn `sensor_ntc::init_door();` loggt das bereits sauber.

## Erwartung nach dem Schritt
- HOST Build: ok
- CLIENT Build: ok
- kein fachlicher Reglerumbau
- Heater läuft weiterhin mit:
  - 4 kHz
  - ca. 50 % Duty
  - niemals 100 %
- Door-Init-Log erscheint nur noch einmal

## Tests
1. HOST bauen
2. CLIENT bauen
3. Bootlog prüfen:
   - `ADS1115 found`
   - genau **einmal** `DOOR init done`
4. Heater Smoke-Test:
   - wenn Host HEATER setzt, erscheint
     - `[HEATER] PWM attached...`
   - beim Abschalten
     - `[HEATER] PWM detached (stopped)`
5. Door-Safety-Test:
   - Door öffnen während Heater aktiv
   - Erwartung:
     - Heater wird abgeschaltet
     - Motor wird abgeschaltet
     - DOOR Bit im STATUS bleibt korrekt

## Kurze Zusammenfassung
In diesem Schritt wird die physische Heater-PWM-Ansteuerung aus `FSD_Client.cpp`
in ein eigenes produktives Client-Modul `heater_io` verschoben.
`FSD_Client.cpp` bleibt damit auf Sicherheitslogik und Zustandsentscheidung fokussiert.

## Commit
`T16.Client.1.1 extract heater PWM into client heater_io module`
