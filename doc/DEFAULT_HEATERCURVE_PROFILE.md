# Default HeaterCurve Profile

Stand: aktueller Default-Code-Stand

Hinweis:

- Diese Tabelle zeigt die aktuellen Default-Werte aus dem Source-Code.
- `hysteresis`, `approachBand`, `holdBand` und `overshootCap` koennen zur Laufzeit ueber `host_parameters` ueberschrieben werden.
- Die Tabelle beschreibt also den Default-Fallback bzw. Initialzustand im Code.

| Parameter | `LOW_45C` | `MID_60C` | `HIGH_80C` | `SILICA_100C` |
|---|---:|---:|---:|---:|
| MaterialClass | `FILAMENT` | `FILAMENT` | `FILAMENT` | `SILICA` |
| Typische Presets | `PLA`, `TPU`, `PVA`, `BVOH` | `PETG`, `HIPS`, `PP` | `ABS`, `ASA`, `PA`, `PC`, `PPS` | `SILICA` |
| Preset-Temperaturbereich | `42.5..52.5°C` | `55.0..65.0°C` | `70.0..85.0°C` | `105.0°C` |
| Hysterese | `1.5°C` | `1.5°C` | `1.5°C` | `2.5°C` |
| Approach-Band | `10.0°C` | `10.0°C` | `10.0°C` | `10.0°C` |
| Hold-Band | `4.0°C` | `4.0°C` | `4.0°C` | `2.5°C` |
| Overshoot-Cap | `+2.0°C` | `+2.0°C` | `+2.0°C` | `+3.0°C` |
| Chamber-Max | `120.0°C` | `120.0°C` | `120.0°C` | `120.0°C` |
| Hotspot-Max | `140.0°C` | `140.0°C` | `140.0°C` | `140.0°C` |
| First Pulse Max | `10000 ms` | `11000 ms` | `12000 ms` | `45000 ms` |
| Bulk Pulse Max | `10000 ms` | `10000 ms` | `10000 ms` | `18000 ms` |
| Approach Pulse Max | `7000 ms` | `7000 ms` | `7000 ms` | `12000 ms` |
| Hold Pulse Max | `6000 ms` | `6000 ms` | `6000 ms` | `8000 ms` |
| First Soak | `45000 ms` | `45000 ms` | `45000 ms` | `60000 ms` |
| Reheat Soak | `30000 ms` | `30000 ms` | `30000 ms` | `35000 ms` |
| Safety Soak | `90000 ms` | `90000 ms` | `90000 ms` | `120000 ms` |
| Bulk Enable Below Target | `20.0°C` | `20.0°C` | `20.0°C` | `25.0°C` |
| Approach Enable Below Target | `10.0°C` | `10.0°C` | `10.0°C` | `12.0°C` |
| Reheat Enable Below Target | `3.0°C` | `3.0°C` | `2.0°C` | `4.0°C` |
| Force-Off Before Target | `1.0°C` | `1.0°C` | `1.0°C` | `1.0°C` |
| First-Pulse Force-Off Before Target | `2.0°C` | `2.0°C` | `2.0°C` | `2.0°C` |
| Hotspot Reheat Block | `+5.0°C` | `+5.0°C` | `+5.0°C` | `n/a` |
| Hotspot Force-Off | `+10.0°C` | `+10.0°C` | `+10.0°C` | `n/a` |
| Fan Min Switch | `5000 ms` | `5000 ms` | `5000 ms` | `n/a` |
| Fan Fast After Heat | `12000 ms` | `12000 ms` | `12000 ms` | `n/a` |
| Resume Soak | `12000 ms` | `12000 ms` | `12000 ms` | `n/a` |
| Resume Min Soak | `5000 ms` | `5000 ms` | `5000 ms` | `n/a` |
| Resume Hot-Target Soak | `7000 ms` | `7000 ms` | `7000 ms` | `n/a` |
| Resume Short Pulse | `6000 ms` | `6000 ms` | `6000 ms` | `n/a` |
| Resume Long Pulse | `8000 ms` | `8000 ms` | `8000 ms` | `n/a` |

## Kurzfazit

- `LOW_45C` und `MID_60C` unterscheiden sich aktuell nur leicht.
- `HIGH_80C` hat bereits frueheres Reheat als die beiden darunter.
- `SILICA_100C` ist klar als eigenes High-Temp-Profil aufgebaut.
