# T16.Client.1.0 – Fix / echte Sensor-Migration

Ursache des aktuellen Verhaltens:
- ADS1115 wird zwar gefunden
- aber STATUS liefert `adc=[0,0,0,0]` und `-32768`
- damit ist die neue produktive Sensorik noch nicht wirklich angebunden

Das ist mein Fehler aus dem letzten Schritt:
- der vorherige Patch war strukturell, aber noch nicht vollständig fachlich migriert

## Einspielen

Dateien aus diesem ZIP:
- `include/client/sensor_ntc.h`
- `src/client/sensor_ntc.cpp`

## Zusätzliche Änderungen in `src/client/FSD_Client.cpp`

### 1) Include ergänzen
Direkt nach:
```cpp
#include "sensors/ads1115_config.h"
```
ergänzen:
```cpp
#include "client/sensor_ntc.h"
```

### 2) Funktion `isDoorOpen()` ersetzen
Durch:
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

### 3) `fillStatusCallback(...)` komplett ersetzen
Durch:
```cpp
static void fillStatusCallback(ProtocolStatus &st) {
    st.outputsMask = g_effectiveMask;

    const bool door_open = sensor_ntc::is_door_open();
    if (door_open) {
        st.outputsMask |= (1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    } else {
        st.outputsMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    }

    sensor_ntc::sample_temperatures();
    const sensor_ntc::Sample &sample = sensor_ntc::get_sample();

    st.adcRaw[0] = sample.rawHotspot;
    st.adcRaw[1] = sample.rawChamber;
    st.adcRaw[2] = 0;
    st.adcRaw[3] = 0;

    st.tempHotspot_dC = sample.hotValid ? sample.hot_dC : ntc::TEMP_INVALID_DC;
    st.tempChamber_dC =
        (sample.cha_dC == ntc::TEMP_INVALID_DC) ? ntc::TEMP_INVALID_DC : sample.cha_dC;
}
```

### 4) ADS1115-Init in `setup()` ersetzen
Den bisherigen Block unter:
```cpp
#if ENABLE_INTERNAL_NTC
...
#endif
```
ersetzen durch:
```cpp
#if ENABLE_INTERNAL_NTC
    sensor_ntc::init_i2c_and_ads();
#endif
```

### 5) Door-Init-Log am Ende von `setup()` ersetzen
Den bisherigen manuellen Door-Init-Log ersetzen durch:
```cpp
    sensor_ntc::init_door();
```

## Erwartung nach dem Fix
- ADS1115 wird wie bisher gefunden
- Türstatus bleibt korrekt
- `adcRaw[0]` und `adcRaw[1]` sind nicht mehr 0
- `tempChamber_dC` wird plausibel
- `tempHotspot_dC` wird plausibel, sofern der Hotspot-Messpfad elektrisch gültig ist

## Tests
1. HOST Build
2. CLIENT Build
3. Bootlog:
   - `ADS1115 found`
   - `DOOR init done`
4. STATUS prüfen:
   - `adc=[x,y,0,0]` mit echten Werten
   - `tempChamber_dC != -32768`
5. Tür öffnen/schließen:
   - DOOR Bit weiterhin korrekt

## Commit
`T16.Client.1.0 migrate real dual-NTC sensor path into client`
