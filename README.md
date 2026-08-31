# Exhaust-Mic

Zweikanalige Tonaufnahme von Motor- und Auspuffgeräuschen für die Hayabusa.
Zwei Mikrofonköpfe mit 135 dBSPL Grenzschalldruck an einem 24-bit-Stereo-ADC,
aufgezeichnet von einem ESP32-S3 auf microSD, mit Zeitstempeln zur
Synchronisation mit Videomaterial.

Auslegung, Rauschbudget, Pegelplan und Begründung der Bauteilwahl:
[`docs/superpowers/specs/2026-08-28-exhaust-mic-design.md`](../../docs/superpowers/specs/2026-08-28-exhaust-mic-design.md)

## Inhalt

| Pfad | Inhalt |
|---|---|
| `exhaust-mic.kicad_sch` | Wurzelblatt der Hauptplatine |
| `01-power.kicad_sch` | Bordnetzeingang, Schutz, LM5164-Buck, LDOs, Zündungserkennung |
| `02-analog.kicad_sch` | Mikrofonstecker, EMV-Filter, Ankopplung an den ADC |
| `03-adc.kicad_sch` | PCM1863 und 24,576-MHz-Audiotakt |
| `04-mcu.kicad_sch` | ESP32-S3-WROOM-1, USB-C, microSD |
| `05-io.kicad_sch` | DS3231-RTC, CAN, Bedienung, Sync-Marker |
| `exhaust-mic.kicad_pcb` | Layout der Hauptplatine, 72 × 55 mm, vier Lagen |
| `mic-head/` | Eigenes Projekt für den Mikrofonkopf, 2× identisch bestückt |
| `lib/exhaust-mic.kicad_sym` | Symbole, die KiCad nicht mitbringt |
| `lib/exhaust-mic.pretty/` | Footprints, die KiCad 7 nicht mitbringt |
| `output/bom-*.csv` | Stücklisten |
| `output/*-gerber.zip` | Fertigungsdaten |
| `tools/` | Generatoren, siehe unten |

## Schaltpläne neu erzeugen

Die Schaltpläne werden aus Python erzeugt statt von Hand gezeichnet. Das hält
Pinbelegung und Netznamen an einer Stelle und macht Änderungen nachvollziehbar.

```sh
python3 tools/build_lib.py        # lib/exhaust-mic.kicad_sym
python3 tools/build_mic_head.py   # mic-head/mic-head.kicad_sch
python3 tools/build_main.py       # Hauptplatine, alle sechs Blätter
```

`build_main.py` meldet beim Lauf jede Leitung, die über einen Bauteilpin
hinwegläuft — solche Stellen sind fast immer versehentliche Kurzschlüsse und
im gedruckten Plan nicht zu sehen. Ein sauberer Lauf gibt keine Warnungen aus.

## Platinen neu erzeugen

Das Layout entsteht auf demselben Weg: `pcbgen.py` kapselt die pcbnew-API,
die beiden `build_*_pcb.py` beschreiben Kontur, Bestückung und Kupferflächen,
`route.py` verlegt die Leiterbahnen mit freerouting.

```sh
kicad-cli sch export netlist --output output/main.net exhaust-mic.kicad_sch
python3 tools/build_pcb.py output/main.net          # Bestückung + Flächen
python3 tools/route.py exhaust-mic.kicad_pcb <arbeitsverzeichnis>
python3 tools/build_outputs.py                      # Gerber, Bohrdaten, PDF
```

`build_pcb.py --map` zeichnet eine grobe ASCII-Belegungskarte je Seite —
damit lässt sich die Aufteilung beurteilen, ohne KiCad zu öffnen.

Große und mechanisch gebundene Teile stehen im Bestückungsplan fest, alles
andere sortiert `pack()` regalweise in benannte Regionen. Das überlebt
Änderungen an einzelnen Footprints, ohne dass 127 Koordinaten nachgeführt
werden müssen.

Für `route.py` muss `fr-2.1.0.jar` (freerouting) im Arbeitsverzeichnis
liegen. Die Einstellung `optimizer.max_passes` ist dabei nicht optional:
`router.max_passes` begrenzt nur die Routing-Durchgänge, danach optimiert
freerouting **unbegrenzt** weiter und schreibt die SES-Datei erst ganz am
Ende. Wer den Prozess von außen abbricht, verliert das komplette Routing.
Weder `job_timeout` noch ein niedriges `max_passes` helfen — nur die
Deckelung des Optimierers.

## Prüfen

```sh
kicad-cli sch export netlist --output /tmp/exhaust-mic.net exhaust-mic.kicad_sch
kicad-cli sch export pdf     --output /tmp/exhaust-mic.pdf exhaust-mic.kicad_sch
python3 tools/build_bom.py /tmp/exhaust-mic.net output/bom-mainboard.csv
```

Die Netzliste ist die eigentliche Abnahme des Schaltplans: jedes Netz und
jeder offene Pin lassen sich gegen Abschnitt 7 und 10 der Spec abgleichen.
Aktuell bleiben 20 Pins bewusst offen (Strapping-Pins des ESP32, unbenutzte
Analogeingänge des PCM1863, die laut Datenblatt ausdrücklich nicht beschaltet
werden dürfen, und ungenutzte Funktionspins).

Für das Layout gibt es kein `kicad-cli pcb drc` in Version 7. Die Abnahme
läuft deshalb über drei eigene Prüfungen in `pcbgen.py`, die bei jedem Lauf
mitlaufen:

| Prüfung | Was sie findet |
|---|---|
| `check()` | Courtyard-Überschneidungen, Teile über der Kontur oder über einer Bohrung, Pads ohne Netz |
| `connectivity_report()` | getrennte Kupferinseln je Netz — KiCad 7 gibt über Python nur die Gesamtzahl heraus, nicht welche |
| `drc_report()` | Abstände Kupfer gegen Kupfer und gegen die Platinenkante |

`drc_report()` modelliert Pads als Strecke mit halber Breite statt als
Umkreis. Mit dem Umkreis meldet jedes Nachbarpad eines SOIC-Gehäuses eine
Verletzung, und echte Fehler gehen im Rauschen unter.

## Dateiformat

Erzeugt wird KiCad-7-Format (`20230121`), weil die lokale Installation
7.0.11 ist und die Pläne damit prüfbar bleiben. KiCad 9 und 10 öffnen die
Dateien und migrieren sie beim ersten Speichern.

## Gehäuse

Die Dichtheit macht das Gehäuse, nicht die Platinensteckverbinder: Superseal
1.0 gibt es nur mit 26, 34 oder 60 Wegen als Platinenheader, die kleinen 5-
und 6-poligen sind reine Kabelsteckverbinder. Auf der Platine sitzen deshalb
JST XH, und die Superseal-Trennstellen wandern ins Kabel.

Vorgesehen ist ein IP67-Kunststoffkasten von etwa 80 × 62 × 25 mm mit
Kabelverschraubungen. Zwei Punkte sind dabei bindend:

- Die Antenne des ESP32-Moduls ragt 6 mm über die linke Platinenkante. Dort
  muss das Gehäuse ausgespart und metallfrei sein. Bei einem Metallgehäuse
  stattdessen den pinkompatiblen ESP32-S3-WROOM-1**U** mit externer Antenne.
- Die microSD-Karte ist nur bei geöffnetem Deckel erreichbar. Für den
  laufenden Betrieb ist der Download über WLAN vorgesehen.

## Stand

Schaltplan, Stückliste und Layout beider Platinen sind fertig und geprüft.
Offen sind Gehäuse und Firmware.
