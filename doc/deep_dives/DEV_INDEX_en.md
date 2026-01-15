# Deep-Dive Documentation (Developer)

These pages are intended for developers who want to **understand, fork, or extend** the project.
They complement the management-focused README and deliberately go deeper into architecture, workflows, states, and screen-specific behavior.

> Note: Conversation and documentation are primarily in German. Source code, comments, and technical artifacts remain in English.

## Contents

1. [Architecture Overview](Dev/01_architektur.md)
2. [Workflow: UI ↔ oven ↔ HostComm](Dev/02_workflows.md)
3. [States and Flags (RUNNING, WAITING, POST, Link/Alive)](Dev/03_zustaende.md)
4. [screen_main – Runtime View](Dev/04_screen_main.md)
5. [screen_config – Configuration](Dev/05_screen_config.md)
6. [screen_dbg_hw – Hardware Debug Screen (Safety)](Dev/06_screen_dbg_hw.md)

## Reference: Development Phases (T1–T8)

- T1–T2: Foundations (display/touch, LVGL setup, initial architecture approach)
- T3–T8: Iterative implementation (UI, workflows, communication, test screen, navigation)

The detailed phase summaries are located under `Docs/`:

- [Development Phase T3 – Architecture / UX](../ESP32-S3_UI_T3_Zusammenfassung.md)
- [Development Phase T4 – screen_main](../ESP32-S3_UI_T4_Zusammenfassung.md)
- [Development Phase T5 – oven logic](../ESP32-S3_UI_T5_Zusammenfassung.md)
- [Development Phase T6 – host/client communication (architecture/test)](../ESP32-S3_UI_T6_Zusammenfassung.md)
- [Development Phase T7 – host/client UX integration](../ESP32-S3_UI_T7_Zusammenfassung.md)
- [Development Phase T8 – screen_dbg_hw](../ESP32-S3_UI_T8_Zusammenfassung.md)

Information:
> screen_log is only a placeholder and has not been implemented<br>
> Extensive logging exists both on the ESP32-S3 and the ESP32-WROOM side.

## Mermaid Notes

Mermaid can be sensitive to special characters. Diagram labels are therefore often wrapped in **quotation marks**.
