# Screen `Parameter`

## Zweck

Der Screen `Parameter` ersetzt den bisherigen ungenutzten LOG-Screen.

Er dient zur Pflege von hostseitigen Grundparametern fuer den Airfryer:

- Fast-Shortcut-Belegung der vier Filament-Tasten aus `screen_main`
- Heater-Profilparameter fuer die vier Host-Heater-Presets `45C`, `60C`, `80C`, `100C`

Alle Aenderungen gelten nur auf dem HOST.

## UX / Layout

- Fester Header mit Titel `Parameter`
- Vertikal scrollbarer Inhaltsbereich fuer zukuenftig wachsende Parameterlisten
- Fester Footer mit:
  - `RESET` links, rot hinterlegt
  - `SAVE` rechts
- Eigene visuelle Gruppen mit abgerundetem Rechteck:
  - `Filament-ShortCuts`
  - `Heater-Curve`
- Horizontaler Screen-Wechsel bleibt ueber den dedizierten Swipe-Bereich erhalten

## Filament-ShortCuts

Vier Shortcut-Slots `F1..F4` werden angezeigt.

Jeder Slot besitzt:

- eine symbolische Taste analog zu `screen_main`
- ein numerisches Spinbox-Stepper-Feld fuer die Preset-Auswahl
- eine automatisch verkuerzte Beschriftung mit maximal 5 Zeichen

Die Shortcut-Tasten in `screen_main` lesen diese Zuordnung jetzt aus dem Host-Parameterstore.

## Heater-Curve

Fuer jedes Heater-Profil existiert eine eigene Karte:

- `45C`
- `60C`
- `80C`
- `100C`

Pro Profil werden aktuell folgende Parameter gepflegt:

- `TGT` Zieltemperatur
- `HYS` Hysterese
- `APR` Approach-Band
- `HLD` Hold-Band
- `OVR` Overshoot-Cap

Die Host-Heater-Policies in `oven.cpp` verwenden die gespeicherten Werte jetzt zur Laufzeit.

## Persistenz

Die Parameter werden im ESP32-Preferences/NVM des HOST gespeichert.

- Namespace: `host-params`
- Key: `cfg`
- Format: versionierter Byte-Blob

Beim Start wird der Cache initialisiert. Ungueltige oder fehlende Daten werden durch Default-Werte ersetzt.

## RESET / SAVE

- `RESET` setzt nur die UI-Werte auf Factory-Defaults zurueck
- Erst `SAVE` speichert die aktuell sichtbaren Werte in NVM

## Betroffene Laufzeitbereiche

- `screen_parameters`: Bearbeitung, Dirty-State, Save/Reset
- `screen_main`: Fast-Shortcut-Tasten
- `screen_config`: effektive Profil-Zieltemperatur beim Laden eines Presets
- `oven.cpp`: Heater-Policy und effektive Preset-Zieltemperatur
