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
| `mic-head/` | Eigenes Projekt für den Mikrofonkopf, 2× identisch bestückt |
| `lib/exhaust-mic.kicad_sym` | Symbole, die KiCad nicht mitbringt |
| `output/bom-*.csv` | Stücklisten |
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

## Prüfen

```sh
kicad-cli sch export netlist --output /tmp/exhaust-mic.net exhaust-mic.kicad_sch
kicad-cli sch export pdf     --output /tmp/exhaust-mic.pdf exhaust-mic.kicad_sch
python3 tools/build_bom.py /tmp/exhaust-mic.net output/bom-mainboard.csv
```

Die Netzliste ist die eigentliche Abnahme: jedes Netz und jeder offene Pin
lassen sich gegen Abschnitt 7 und 10 der Spec abgleichen. Aktuell bleiben
20 Pins bewusst offen (Strapping-Pins des ESP32, unbenutzte Analogeingänge des
PCM1863, die laut Datenblatt ausdrücklich nicht beschaltet werden dürfen, und
ungenutzte Funktionspins).

## Dateiformat

Erzeugt wird KiCad-7-Format (`20230121`), weil die lokale Installation
7.0.11 ist und die Pläne damit prüfbar bleiben. KiCad 9 und 10 öffnen die
Dateien und migrieren sie beim ersten Speichern.

## Stand

Schaltplan und Stückliste sind fertig und netzlistengeprüft. Offen sind
Layout, Gehäuse und Firmware.
