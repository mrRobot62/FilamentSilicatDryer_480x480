Sehr gute Idee 👍 – genau so sollte T5 sauber abgeschlossen werden.

Ich gebe dir jetzt eine strukturierte, saubere Zusammenfassung für ESP32_S3_UI_T5.md, so geschrieben,
dass du (oder ich) Wochen später direkt mit T6 weiterarbeiten kannst, ohne diesen Chat lesen zu müssen.

Du kannst den folgenden Inhalt 1:1 in die Markdown-Datei übernehmen.

⸻

ESP32-S3 UI – T5 (Config Screen & Runtime Preset Handling)

Projektkontext

Dieses Dokument fasst den Abschlussstand von UI-Iteration T5 zusammen.
T5 fokussiert sich auf den Config-Screen (Preset-Auswahl + Anpassung von Zeit & Temperatur) sowie die saubere Übergabe der Runtime-Daten an den Main-Screen.

Ziel war kein Persistieren von Presets, sondern ein laufzeitbezogenes CurrentPreset, das beim Start verwendet wird.

⸻

Architektur – Grundprinzip

Presets vs Runtime
	•	Presets (FilamentPreset)
	•	static constexpr in oven.h
	•	niemals veränderbar
	•	Enthalten Default-Werte:
	•	Name
	•	Temperatur
	•	Dauer
	•	Rotary-Flag
	•	Runtime (OvenRuntimeState)
	•	Lebt nur zur Laufzeit
	•	Wird vom Config-Screen manipuliert
	•	Wird vom Main-Screen genutzt
	•	Wird beim START verwendet

➡️ Der Config-Screen arbeitet ausschließlich auf der Runtime, nie direkt auf Presets.

⸻

Datenmodell (relevant)

FilamentPreset

typedef struct {
    const char *name;
    float dryTempC;
    uint16_t durationMin;
    bool rotaryOn;
} FilamentPreset;

OvenRuntimeState (Auszug)

typedef struct {
    uint32_t durationMinutes;
    uint32_t secondsRemaining;

    float tempCurrent;
    float tempTarget;

    int filamentId;
    char presetName[24];
    bool rotaryOn;

    bool running;
} OvenRuntimeState;


⸻

Screen-Flow

screen_main
    ↓ swipe
screen_config
    - Preset wählen
    - HH/MM ändern
    - Temperatur ändern
    - SAVE
    ↓
screen_main
    - START nutzt Runtime


⸻

Config-Screen – UI Aufbau (T5)

Karten
	•	FILAMENT
	•	Roller mit Preset-Namen
	•	TIME
	•	HH-Roller (0–24)
	•	MM-Roller (0–55 in 5-Min-Schritten)
	•	TEMP
	•	Temperatur-Roller (0–120 °C)

Wichtiges Layout-Learning
	•	Kein time_row mehr verwenden
	•	Roller direkt in time_content platzieren
	•	Kein LV_SIZE_CONTENT in verschachtelten Flex-Layouts
	•	Feste Höhen (kRollerH) verwenden
	•	Flex nur dort einsetzen, wo nötig

➡️ Diese Änderungen haben den Hard-Crash beim Swipe behoben

⸻

Runtime-Synchronisation (zentrales Konzept)

Zentrale Funktion

static void apply_runtime_from_widgets();

Diese Funktion ist der Single Source of Truth, um UI → Runtime zu synchronisieren.

Sie:
	•	liest alle Roller
	•	berechnet Minuten korrekt (HH + MM*5)
	•	setzt Runtime über:
	•	oven_select_preset()
	•	oven_set_runtime_duration_minutes()
	•	oven_set_runtime_temp_target()

⸻

Event-Handling (T5 final)

Filament-Roller

static void filament_roller_event_cb(lv_event_t *e)

Ablauf:
	1.	oven_select_preset(index)
	2.	load_preset_to_widgets(index)
	3.	apply_runtime_from_widgets()

➡️ Runtime ist sofort konsistent, auch ohne weitere Änderungen.

⸻

HH / MM / TEMP Roller

static void hh_roller_event_cb(lv_event_t *e)
static void mm_roller_event_cb(lv_event_t *e)
static void temp_roller_event_cb(lv_event_t *e)

Alle:
	•	rufen nur apply_runtime_from_widgets()
	•	keine Preset-Modifikation

⸻

SAVE-Button

static void btn_save_event_cb(lv_event_t *e)

Ablauf:
	1.	Blockiert, wenn Ofen läuft
	2.	apply_runtime_from_widgets() (finaler Sync)
	3.	Rückkehr zu screen_main

➡️ SAVE bedeutet: Runtime ist final

⸻

Screen-Manager Verhalten
	•	Screens werden nicht neu erzeugt
	•	screen_manager_go_home() blendet nur um
	•	Runtime bleibt erhalten
	•	Main-Screen liest Runtime live

⸻

Debug- & Entwicklungs-Hilfen

Debug-Frames
	•	Per Compiler-Directive aktivierbar
	•	1px Border mit Radius
	•	Für:
	•	root
	•	top / center / bottom
	•	left / middle / right
	•	cards

Layout-Diagnose
	•	lv_obj_update_layout() gezielt genutzt
	•	Größen/Koordinaten geloggt
	•	Ursache des Crashs:
	•	invalid geometry während Swipe
	•	Flex + Auto-Height in verschachtelten Containern

⸻

Status am Ende von T5 ✅

✔ Filament auswählen
✔ Preset-Defaults laden
✔ Zeit & Temperatur ändern
✔ Runtime korrekt aktualisiert
✔ SAVE übernimmt Runtime
✔ START im Main-Screen nutzt Runtime
✔ Keine Crashes beim Swipe
✔ Saubere Card-Layouts

⸻

Offene Themen für T6

Funktional
	•	Persistenz (NVS) optional
	•	Optional: „Reset to Preset“ Button
	•	Optional: Preset-Name Anzeige im Config-Header

UI
	•	Feintuning Padding / Card-Abstände
	•	Animationen beim Preset-Wechsel
	•	Disable/Enable Logik bei oven_is_running()

Architektur
	•	Trennung RuntimeProfile vs OvenRuntimeState (optional)
	•	Unit-Test für apply_runtime_from_widgets()

⸻

Merksatz für T6

Presets sind Read-Only.
Der Config-Screen schreibt ausschließlich Runtime.
Der Main-Screen liest ausschließlich Runtime.

⸻

Wenn du möchtest, können wir als nächsten Schritt in T6:
	•	entweder Persistenz sauber designen (NVS)
	•	oder UI-Feinschliff + Disable-Logik bei RUNNING
	•	oder Runtime-Preview im Main-Screen (Live-Update)

Sag mir einfach, womit wir T6 beginnen 👍