# Changelog

## 0.7.0 - 2026-04-06

- Neuer Screen `Parameter` ersetzt den ungenutzten LOG-Screen.
- Fast-Shortcut-Zuordnung der vier Filament-Tasten ist jetzt hostseitig konfigurierbar und wird in NVM gespeichert.
- Heater-Curve-Parameter der Presets `45C`, `60C`, `80C`, `100C` sind jetzt hostseitig konfigurierbar und werden in NVM gespeichert.
- `SAVE` und `RESET` schreiben den aktuellen Stand in NVM und fuehren danach einen Reboot aus.
- `SAVE` und `RESET` besitzen Sicherheitsabfragen im Parameterscreen.
- Neuer Parameterblock `Display timeout` mit:
  - `Dimmfaktor (%)`
  - `Dimm-Timeout (min)`
- Display-Dimmen ist hostseitig konfigurierbar, wird in NVM gespeichert und nach Reboot wieder geladen.
- Erster Touch nach gedimmtem Display hellt nur auf und loest keine UI-Aktion aus.
- Parameterscreen ist per Swipe nur erreichbar, wenn das System nicht `RUNNING` ist.
- Neue Dokumentation fuer Default-HeaterCurve-Profile und den erweiterten Parameterscreen.
