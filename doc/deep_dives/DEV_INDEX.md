# Deep-Dive Dokumentation (Developer)

Diese Seiten richten sich an Entwickler, die das Projekt **verstehen, forken oder erweitern** möchten.
Sie ergänzen die management-taugliche README und gehen bewusst tiefer in Architektur, Workflows, Zustände und Screen-spezifisches Verhalten.

> Hinweis: Konversation und Dokumentation sind primär Deutsch. Quellcode, Kommentare und technische Artefakte bleiben Englisch.

## Inhalt

1. [Architektur-Überblick](Dev/01_architektur.md)
2. [Workflow: UI ↔ oven ↔ HostComm](Dev/02_workflows.md)
3. [Zustände und Flags (RUNNING, WAITING, POST, Link/Alive)](Dev/03_zustaende.md)
4. [screen_main – Laufzeitansicht](Dev/04_screen_main.md)
5. [screen_config – Konfiguration](Dev/05_screen_config.md)
6. [screen_dbg_hw – Hardware-Testscreen (Safety)](Dev/06_screen_dbg_hw.md)

## Referenz: Entwicklungsphasen (T1–T8)

- T1–T2: Grundlagen (Display/Touch, LVGL-Setup, erster Architekturansatz)
- T3–T8: Iterative Implementierung (UI, Workflows, Communication, Testscreen, Navigation)

Die detaillierten Phasen-Zusammenfassungen liegen unter `Docs/`:

- [Entwicklungsphase T3 - Architektur / UX](../ESP32-S3_UI_T3_Zusammenfassung.md)
- [Entwicklungsphase T4 - screen_main](../ESP32-S3_UI_T4_Zusammenfassung.md)
- [Entwicklungsphase T5 - oven-logic](../ESP32-S3_UI_T5_Zusammenfassung.md)
- [Entwicklungsphase T6 - host/client Kommunikation (architektur/test)](../ESP32-S3_UI_T6_Zusammenfassung.md)
- [Entwicklungsphase T7 - host/client UX Integration](../ESP32-S3_UI_T7_Zusammenfassung.md)
- [Entwicklungsphase T8 - screen_dbg_hw](../ESP32-S3_UI_T8_Zusammenfassung.md)

Information:
> screen_log ist lediglich ein Platzhalter und wurde nicht implementiert<br>
> Umfangreiche Logausgaben sowohl über ESP32-S3 als auch über ESP32-Wroom.

## Mermaid-Hinweise

Mermaid kann empfindlich auf Sonderzeichen reagieren. In den Diagrammen werden Texte daher häufig in **Anführungszeichen** gesetzt.
