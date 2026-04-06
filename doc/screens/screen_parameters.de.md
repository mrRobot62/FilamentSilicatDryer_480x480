# Screen `Parameter`

## Zweck

Der Screen `Parameter` ersetzt den bisherigen ungenutzten LOG-Screen.

Er dient zur Pflege von hostseitigen Grundparametern fuer den Airfryer:

- Fast-Shortcut-Belegung der vier Filament-Tasten aus `screen_main`
- Heater-Profilparameter fuer die vier Host-Heater-Presets `45C`, `60C`, `80C`, `100C`
- Display-Timeout-Parameter fuer Dimmverhalten und Timeout

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
  - `Display timeout`
- Horizontaler Screen-Wechsel bleibt ueber den dedizierten Swipe-Bereich erhalten
- Der Screen ist per Swipe nur erreichbar, solange das System nicht `RUNNING` ist

## Filament-ShortCuts

Vier Shortcut-Slots `F1..F4` werden angezeigt.

Jeder Slot besitzt:

- eine symbolische Taste analog zu `screen_main`
- einen `lv_roller` direkt in der Taste fuer die Preset-Auswahl
- eine automatisch verkuerzte Beschriftung mit maximal 5 Zeichen

Die Shortcut-Tasten in `screen_main` lesen diese Zuordnung jetzt aus dem Host-Parameterstore.

## Heater-Curve

Die Auswahl des Presets erfolgt ueber einen `lv_roller`:

- `45C`
- `60C`
- `80C`
- `100C`

Fuer das gerade ausgewaehlte Preset werden folgende Parameter bearbeitet:

- `Zieltemperatur (°C)` als Integerwert
- `Hysterese (°C)` mit einer Nachkommastelle
- `Anfahrband (°C)` mit einer Nachkommastelle
- `Halteband (°C)` mit einer Nachkommastelle
- `Ueberschwingen (°C)` mit einer Nachkommastelle

Die Werte werden intern als Integer gespeichert:

- Zieltemperatur in `°C`
- alle Temperaturbaender in `0.1 °C`

Die Host-Heater-Policies in `oven.cpp` verwenden die gespeicherten Werte jetzt zur Laufzeit.

## Display timeout

Die Gruppe `Display timeout` verwaltet hostseitig das automatische Dimmen des Displays.

Parameter:

- `Dimmfaktor (%)`
- `Dimm-Timeout (min)`

Verhalten:

- Nach Ablauf des Timeouts wird das Display visuell gedimmt.
- Das Dimmen erfolgt absichtlich nicht ueber Hardware-PWM am Backlight, sondern ueber ein LVGL-Overlay.
- Der erste Touch nach einem gedimmten Display hellt nur auf und wird verworfen.
- Erst ein zweiter Touch darf wieder aktive UI-Aktionen ausloesen.
- Relevante Systemereignisse und Benutzeraktivitaet setzen den Timeout neu.

## Persistenz

Die Parameter werden im ESP32-Preferences/NVM des HOST gespeichert.

- Namespace: `host-params`
- Key: `cfg`
- Format: versionierter Byte-Blob

Beim Start wird der Cache initialisiert. Ungueltige oder fehlende Daten werden durch Default-Werte ersetzt.

## RESET / SAVE

- `SAVE` fragt vor dem Speichern bestaetigend nach
- `RESET` fragt vor dem Laden der Werkeinstellungen bestaetigend nach
- `SAVE` speichert die aktuell sichtbaren Werte in NVM und rebootet danach
- `RESET` laedt feste Defaultwerte, speichert diese in NVM und rebootet danach
- Die Defaultwerte im Code bleiben unveraendert und dienen immer als Factory-Stand

## Betroffene Laufzeitbereiche

- `screen_parameters`: Bearbeitung, Dirty-State, Save/Reset
- `screen_main`: Fast-Shortcut-Tasten
- `screen_config`: effektive Profil-Zieltemperatur beim Laden eines Presets
- `oven.cpp`: Heater-Policy und effektive Preset-Zieltemperatur
- `display_timeout_manager`: Dimm-Timeout, Wake-on-Touch und Ereignisreaktion
