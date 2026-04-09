# Changelog

## Unreleased

- added a second PlatformIO environment for `esp32-s3-devkitc-1`
- moved shared PlatformIO settings into a global `[env]` block
- both board targets now use the same UDP logging, CSV logging and client log flags
- demo loop currently emits logs faster, which makes testing with `udp-viewer` easier

## 0.7.1 - 2026-04-09

- `feature/small_enhancements` in `main` migriert
- neuer hostseitiger Screen `Parameter` ersetzt den ungenutzten LOG-Screen
- Fast-Shortcut-Zuordnung der vier Filament-Tasten ist jetzt konfigurierbar und NVM-persistent
- Heater-Curve-Parameter der Presets `45C`, `60C`, `80C`, `100C` sind jetzt hostseitig konfigurierbar und NVM-persistent
- `SAVE` und `RESET` schreiben den aktuellen Stand in NVM und fuehren danach einen Reboot aus
- `SAVE` und `RESET` besitzen Sicherheitsabfragen im Parameterscreen
- neuer Parameterblock `Display timeout` mit `Dimmfaktor (%)` und `Dimm-Timeout (min)`
- Display-Dimmen ist hostseitig konfigurierbar, wird in NVM gespeichert und nach Reboot wieder geladen
- der erste Touch nach gedimmtem Display hellt nur auf und loest keine UI-Aktion aus
- Parameterscreen ist per Swipe nur erreichbar, wenn das System nicht `RUNNING` ist
- README, README_EN und Screen-Dokumentation aktualisiert
