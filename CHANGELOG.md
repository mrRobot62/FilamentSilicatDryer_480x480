# Changelog

## 0.7.3 - 2026-04-12

- hostseitige Langzeit-CSV-Aggregation `CSV_LR_HOST` mit `avg/min/max` fuer Kammer- und Hotspot-Temperatur ergaenzt
- neuer persistenter HOST-Parameterblock fuer `CSV long-term` mit `OFF/ON` und Intervall `10/30/60/300s`
- Parameterscreen um `CSV long-term` erweitert
- NVS-Host-Parameterstruktur auf neue Blob-Version fuer Langzeit-CSV erweitert
- neue Architektur-Doku fuer Langzeit-CSV in `doc/02_software_architecture/04_long_term_csv_sampling.md`
- aktive Hardware-Doku um Setup-, Wiring- und BOM-Baselines erweitert
- README, README_EN und aktive Doku-Einstiegspunkte auf den aktuellen Release-Stand angehoben

## 0.7.2 - 2026-04-09

- Dokumentation auf Basis des aktuellen `main`-Stands neu strukturiert
- bisherige Dokumente und Assets in `doc/archive/pre_reorg_v0.7.1/legacy_docs/` archiviert
- neue aktive Bereiche fuer Hardware, Softwarearchitektur und User Guide aufgebaut
- Mermaid-Diagramme fuer Host-, Client- und Protokollarchitektur ergänzt
- README und README_EN auf die neue Dokumentationsstruktur umgestellt

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
