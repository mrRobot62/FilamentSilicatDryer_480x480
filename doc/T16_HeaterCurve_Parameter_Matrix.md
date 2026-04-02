# T16 HeaterCurve Parameter Matrix

Stand: aktueller Source-Stand nach `T16_Phase_4.1`

## Profil-Zuordnung

- `LOW_45C`: niedrige Filament-Trocknungstemperaturen, z. B. PLA/TPU/BVOH/PVA
- `MID_60C`: mittlere Filament-Trocknungstemperaturen, z. B. PETG/HIPS/PP
- `HIGH_80C`: hohe Filament-Trocknungstemperaturen, z. B. ABS/ASA/PA/PC/PPS
- `SILICA_100C`: Silicagel-Trocknung im Bereich um 100 C

## Parameter-Tabelle

| Parameter | Info | `LOW_45C` | `MID_60C` | `HIGH_80C` | `SILICA_100C` |
|---|---|---:|---:|---:|---:|
| `materialClass` | Grobe Materialgruppe fuer bestehende Speziallogik im Host. | `FILAMENT` | `FILAMENT` | `FILAMENT` | `SILICA` |
| `hysteresisC` | Hysterese im `HOLD`-Bereich der Basis-Policy. Bestimmt, wie weit die Chamber unter Target fallen darf, bevor wieder geheizt wird. | `1.5 C` | `1.5 C` | `1.5 C` | `2.5 C` |
| `approachBandC` | Abstand zum Target, ab dem von `BULK_HEAT` nach `APPROACH` gewechselt wird. | `10.0 C` | `10.0 C` | `10.0 C` | `10.0 C` |
| `holdBandC` | Abstand zum Target, ab dem in den `HOLD`-Bereich gewechselt wird. | `4.0 C` | `4.0 C` | `4.0 C` | `2.5 C` |
| `targetOvershootCapC` | Harte Host-Safety-Grenze relativ zum Target. Wird sie ueberschritten, setzt der Host `safetyCutoffActive`. | `+2.0 C` | `+2.0 C` | `+2.0 C` | `+3.0 C` |
| `chamberMaxC` | Absolute maximale Chamber-Temperatur fuer die Host-Safety. | `120.0 C` | `120.0 C` | `120.0 C` | `120.0 C` |
| `hotspotMaxC` | Absolute maximale Hotspot-Temperatur fuer die Host-Safety. | `140.0 C` | `140.0 C` | `140.0 C` | `140.0 C` |
| `firstPulseMaxMs` | Maximaldauer des allerersten Heizpulses nach Start. Begrenzt den initialen Energieeintrag. | `10000 ms` | `10000 ms` | `11000-12000 ms` | `30000 ms` |
| `bulkPulseMaxMs` | Maximaldauer eines Heizpulses im groben Aufheizbereich weit unter Target. | `10000 ms` | `10000 ms` | `10000 ms` | `18000 ms` |
| `approachPulseMaxMs` | Maximaldauer eines Heizpulses in der Annäherungsphase an das Target. | `7000 ms` | `7000 ms` | `7000 ms` | `12000 ms` |
| `holdPulseMaxMs` | Maximaldauer eines kurzen Nachheizpulses im spaeten Haltebereich. | `6000 ms` | `6000 ms` | `4000-5000 ms` | `8000 ms` |
| `firstSoakMs` | Erzwingt nach dem ersten Heizpuls eine Beobachtungs-/Abkuehlphase ohne neues Heizen. Wichtig gegen Nachlauf-Overshoot. | `45000 ms` | `45000 ms` | `45000 ms` | `90000 ms` |
| `reheatSoakMs` | Erzwingt zwischen Folgepulsen eine Pause, damit das traege System nachlaufen und ausgewertet werden kann. | `30000 ms` | `30000 ms` | `30000 ms` | `45000 ms` |
| `safetySoakMs` | Zusaetzliche Sperrzeit nach einem Safety-Ereignis, bevor wieder geheizt werden darf. | `90000 ms` | `90000 ms` | `90000 ms` | `120000 ms` |
| `bulkPulseEnableBelowTargetC` | Fehler zum Target, ab dem statt kleinerer Pulse noch ein voller Bulk-Puls erlaubt wird. | `20.0 C` | `20.0 C` | `20.0 C` | `25.0 C` |
| `approachPulseEnableBelowTargetC` | Fehler zum Target, ab dem noch ein Approach-Puls statt eines Hold-Pulses erlaubt wird. | `10.0 C` | `10.0 C` | `10.0 C` | `12.0 C` |
| `reheatEnableBelowTargetC` | Chamber muss mindestens um diesen Betrag unter Target liegen, bevor wieder geheizt werden darf. | `3.0 C` | `3.0 C` | `3.0 C` | `5.0 C` |
| `forceOffBeforeTargetC` | Direkte Abschaltreserve vor Target fuer spaetere Pulse. Verhindert, dass bei zu spaeter Abschaltung zu viel Restwaerme nachkommt. | `1.0 C` | `1.0 C` | `1.0 C` | `1.5 C` |
| `firstPulseForceOffBeforeTargetC` | Strengere Abschaltreserve fuer den allerersten Puls, um den ersten Peak zu entschaerfen. | `2.0 C` | `2.0 C` | `2.0 C` | `3.0 C` |
| `hotspotReheatBlockAboveTargetC` | Blockiert bei Filament einen neuen Heizpuls, wenn der Hotspot noch zu weit ueber dem Target liegt. | `+5.0 C` | `+5.0 C` | `+5.0 C` | `n/a` |
| `hotspotForceOffAboveTargetC` | Erzwingt bei Filament Heater-OFF, wenn der Hotspot weit ueber dem Target liegt. | `+10.0 C` | `+10.0 C` | `+10.0 C` | `n/a` |
| `fanMinSwitchMs` | Minimaler Umschaltabstand zwischen `FAN230V` und `FAN230V_SLOW` zur Schonung von Motor und Elektronik. | `5000 ms` | `5000 ms` | `5000 ms` | `n/a` |
| `fanFastAfterHeatMs` | Haelt bei Filament nach einem Heizpuls den schnellen Luefter fuer eine feste Zeit aktiv. | `12000 ms` | `12000 ms` | `12000 ms` | `n/a` |
| `waitResumeSoakMs` | Standard-Sperrzeit nach `WAIT -> RUNNING`, bevor wieder geheizt werden darf. | `12000 ms` | `12000 ms` | `12000 ms` | `n/a` |
| `waitResumeSoakMinMs` | Untergrenze fuer die Resume-Sperrzeit nach `WAIT`. | `5000 ms` | `5000 ms` | `5000 ms` | `n/a` |
| `waitResumeSoakHotTargetMs` | Kuerzere Resume-Sperrzeit fuer heissere Filament-Targets. | `7000 ms` | `7000 ms` | `7000 ms` | `n/a` |
| `waitResumePulseShortMs` | Kurzer Recovery-Puls nach `WAIT`, wenn nur wenig Temperatur zum Target fehlt. | `6000 ms` | `6000 ms` | `6000 ms` | `n/a` |
| `waitResumePulseLongMs` | Laengerer Recovery-Puls nach `WAIT`, wenn deutlich mehr Temperatur zum Target fehlt. | `8000 ms` | `8000 ms` | `8000 ms` | `n/a` |
| `waitResumeLongPulseErrorC` | Temperaturfehler, ab dem nach `WAIT` der lange Recovery-Puls verwendet wird. | `8.0 C` | `8.0 C` | `8.0 C` | `n/a` |
| `waitResumeMediumPulseErrorC` | Temperaturfehler, ab dem ein mittlerer Recovery-Puls verwendet wird. | `5.0 C` | `5.0 C` | `5.0 C` | `n/a` |
| `midTargetC` | Schwelle, ab der Filament-Targets als mittlerer/hoeherer Temperaturbereich behandelt werden. | `70.0 C` | `70.0 C` | `70.0 C` | `n/a` |
| `waitResumeHotTargetC` | Schwelle, ab der Filament-Targets als heiss betrachtet und defensiver resumiert werden. | `80.0 C` | `80.0 C` | `80.0 C` | `n/a` |
| `waitResumeLongOpenMs` | Tueroeffnungsdauer, ab der der Resume-Pfad als laengerer Unterbruch behandelt wird. | `15000 ms` | `15000 ms` | `15000 ms` | `n/a` |
| `controlStrategy` | Beschreibt die aktuell verwendete Regelidee im Host. | `bounded filament pulses + fan hysteresis` | `bounded filament pulses + fan hysteresis` | `bounded filament pulses + fan hysteresis` | `bounded silica pulses, chamber-dominated` |
| `safetyPrimarySensor` | Fuehrende Groesse fuer die Safety-Entscheidung. | `Chamber + Hotspot` | `Chamber + Hotspot` | `Chamber + Hotspot` | `Chamber primär, Hotspot als Zusatz-Cutoff` |
| `reheatDecisionBasis` | Hauptgroesse fuer die Freigabe neuer Heizpulse. | `Chamber + Hotspot-Guards` | `Chamber + Hotspot-Guards` | `Chamber + Hotspot-Guards` | `Chamber` |

## Hinweise

- `LOW_45C`, `MID_60C` und `HIGH_80C` sind architektonisch getrennt, aber fachlich noch nicht vollstaendig unterschiedlich kalibriert.
- Die erste echte spezifische Profil-Regelung wurde in `T16_Phase_4.1` fuer `SILICA_100C` eingefuehrt.
- Die Profilfeinabstimmung fuer `LOW_45C`, `MID_60C` und `HIGH_80C` ist weiterhin offen.

