# Firmware

ESP-IDF v5.3.2, Ziel `esp32s3`. Baut warnungsfrei, 664 kB Abbild.

```sh
git clone --depth 1 -b v5.3.2 --recursive https://github.com/espressif/esp-idf ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh
cd firmware && idf.py set-target esp32s3 && idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Die Pinbelegung wird erzeugt, nicht abgetippt

```sh
python3 firmware/tools/pins_aus_netzliste.py     # -> firmware/main/pins.h
```

Quelle ist `output/main.net`, also der Schaltplan. Der Header ist
mitversioniert: ein Diff zeigt sofort, wenn ein GPIO gewandert ist. Von
Hand geändert wird er nicht.

## Zündung, Motorlauf und Abschalten

Die Platine hängt mit `VBAT12` an Klemme 30, ihre Freigabe `EN/UVLO` aber
an Klemme 15 — Zündung an heißt anlaufen. Zündung aus würde den Strom
mitten im Schreiben abstellen, deshalb hält `PWR_HOLD` (GPIO42) die
Freigabe über D8 fest, bis die Datei zu ist. Der Aufbau steht in
[`docs/entwicklung.md`](../docs/entwicklung.md#die-zündung-schaltet-nicht-das-bordnetz).

`zuendung_selbsthaltung()` ist die **erste Zeile in `app_main`** — vor
allem anderen. Beim Anlassen bricht die Zündspannung ein; ohne die
Selbsthaltung fiele die Platine genau dann aus, wenn die Aufnahme
beginnen soll.

Gemessen wird über die beiden Teiler auf dem Versorgungsblatt:

| | Teiler | Bereich |
|---|---|---|
| Klemme 15 | R9 100k / R10 47k, Zenerklemme D5 | brauchbar bis rund 10 V, darüber gekappt |
| Bordnetz | R11 470k / R12 68k | ungekappt bis über 20 V |

Klemme 15 ist damit ein Anwesenheitsmelder, das Bordnetz eine Messung.
Daraus die drei Zustände:

| Zustand | Bedingung | Wirkung |
|---|---|---|
| `ZUENDUNG_AUS` | Klemme 15 unter 5 V für 2 s | Datei schließen, Selbsthaltung lösen |
| `ZUENDUNG_AN` | Klemme 15 über 7 V für 0,3 s | bereit, keine Aufnahme |
| `ZUENDUNG_MOTOR` | Bordnetz über 13,2 V für 2 s | Aufnahme läuft |

**Der Motor ist der dritte Auslöser** neben Taster und Kamera: springt er
an, hebt die Lichtmaschine das Bordnetz von der Ruhespannung (12,6 bis
12,8 V, direkt nach der Fahrt bis 13,0 V) auf 14,0 bis 14,4 V. 13,2 V
trennt beides. Zurück geht es erst unter 12,9 V und nach zehn Sekunden —
im Leerlauf mit Lüfter und Licht bricht die Spannung sonst kurz genug
ein, um die Aufnahme zu zerhacken.

Die Abschaltschwelle für Klemme 15 liegt mit 5 V bewusst tief: beim
Anlassen geht die Zündleitung mit dem Bordnetz in die Knie, und wer hier
9 V ansetzt, schaltet das Gerät genau beim Motorstart ab. Der
Schlüsseldreh dagegen fällt hart auf null.

Am Schreibtisch stört das nichts: `+4V6` kommt über D4 auch aus
`USB_VBUS`, und der Automat schaltet nur ab, wenn er die Zündung vorher
einmal gesehen hat. `PGOOD` sagt zusätzlich, ob der Abwärtswandler
regelt.

⚠️ **Die Werkskalibrierung des ADC ist Pflicht.** Ohne den eFuse-Wert
liegen die Spannungen um bis zu zehn Prozent daneben — das ist genau der
Abstand zwischen Ruhe- und Ladespannung. `zuendung_start()` gibt dann
einen Fehler zurück, statt zu raten.

## GoPro

### Was geht: Aufnahme automatisch mitlaufen lassen

Über Bluetooth LE nach der Open-GoPro-Schnittstelle. Der Rekorder meldet
sich für Statusänderungen an; sobald die Kamera zu kodieren beginnt,
startet die Aufnahme, und umgekehrt. Der Taster am Gerät löst zusätzlich
die Kamera aus.

Alle Kennungen stammen aus dem offiziellen SDK
(`open_gopro/models/constants/`), nicht aus dem Gedächtnis:

| | Wert |
|---|---|
| Dienst | `0000FEA6-0000-1000-8000-00805F9B34FB` |
| Merkmale | `b5f9XXXX-aa8d-11e3-9046-0002a5d5c51b`, XXXX = 0072 Befehl, 0073 Antwort, 0076 Abfrage, 0077 Antwort |
| Auslöser | `SET_SHUTTER` 0x01 |
| Statusabo | `REG_STATUS_VAL_UPDATE` 0x53, Rückmeldung `STATUS_VAL_PUSH` 0x93 |
| Status | `ENCODING` = 10 |

Die Kamera muss beim ersten Mal im Kopplungsmodus stehen
(Einstellungen → Verbindungen → Gerät verbinden).

### Was nicht geht: die Kamera mit Ton beliefern

**Auf dieser Platine unmöglich, aus zwei voneinander unabhängigen
Gründen.**

1. **Der ESP32-S3 kann kein Bluetooth Classic.** Er beherrscht
   ausschließlich Bluetooth LE 5.0; Espressif schreibt für beide
   Host-Stapel ausdrücklich „Classic Bluetooth is not supported". Damit
   gibt es weder A2DP noch HFP. GoPro nimmt drahtlosen Ton aber genau
   über HFP an — so werden AirPods und die DJI Mic 2 im Bluetooth-Modus
   eingebunden.
2. **Selbst wenn es ginge, wäre es ein Rückschritt.** HFP ist ein
   Telefonieprofil: einkanalig, 8 oder 16 kHz. Diese Platine existiert für
   zwei Kanäle mit 24 Bit bei 48 kHz und 135 dBSPL Grenzschalldruck. Der
   Weg über HFP würfe praktisch die gesamte Auslegung weg.

Zwei Auswege, falls der Ton wirklich in die Kameradatei soll:

- **Verkabelt.** GoPro Media Mod oder der 3,5-mm-Adapter am USB-C. Braucht
  einen Analogausgang auf der Platine — den gibt es heute nicht, das wäre
  eine Rev B.
- **Getrennt aufnehmen und im Schnitt zusammenlegen.** Genau dafür ist die
  Zeitbasis gebaut: BWF-Zeitstempel in jeder Datei, Sekundenanker in der
  Begleitdatei, dazu Blitz und Piep als framegenaue Marke. Resolve und
  Premiere ziehen die Spur von selbst an die richtige Stelle. Mit der
  BLE-Kopplung oben passiert das ohne Handgriff.

## Stand gegen die Anforderungen der Spezifikation

| | Anforderung | Stand |
|---|---|---|
| F1 | I2S-Slave-RX per DMA | ✅ `audio.c` |
| F2 | Vorallokierte Datei, Kopf beim Schließen gepatcht | ✅ `speicher.c` |
| F3 | BWF/BEXT mit RTC-Zeit | ✅ `speicher.c` |
| F4 | 1-Hz-Anker latcht den Samplezähler | ✅ `zeitbasis.c` |
| F5 | PCM1863 über I²C, differentiell, Taktherr | ✅ `pcm1863.c` |
| F6 | Übersteuerung mit Anzeige | ✅ Anzeige; automatische Absenkung offen |
| F7 | Zündungsautomat, sauberes Herunterfahren | ✅ `zuendung.c` |
| F8 | Sync-Marker Blitz + Piep, Samplenummer | ✅ `bedienung.c` |
| F9 | WLAN nur außerhalb der Aufnahme, NTP | ❌ offen |
| F10 | CAN über TWAI in die Begleitdatei | ❌ offen |
| F11 | Download über USB-MSC oder WLAN | ❌ offen |
| — | GoPro-Kopplung über BLE | ✅ `gopro.c` |

**Nichts davon lief bisher auf echter Hardware** — es gibt noch keinen
Aufbau. Geprüft ist bisher nur, dass es übersetzt. Die Registerwerte des
PCM1863 stammen aus dem Linux-Treiber `sound/soc/codecs/pcm186x.{c,h}`,
die I²C-Adresse 0x4A folgt aus MS/AD an Masse.

## Aufbau

| Datei | Inhalt |
|---|---|
| `main.c` | Zustandsführung, Aufnahme starten und stoppen |
| `gopro.c` | BLE-Zentral, Open-GoPro-Protokoll |
| `audio.c` | I2S-Empfang, Rahmenzähler, Spitzenwert |
| `pcm1863.c` | Wandler über I²C |
| `speicher.c` | Karte, WAV mit BEXT, Begleitdatei |
| `zeitbasis.c` | DS3231, Sekundenanker |
| `zuendung.c` | Klemme 15 und Bordspannung, Selbsthaltung, Abschalten |
| `bedienung.c` | Taster, Leuchtdioden, Piezo, Marker |
| `i2cbus.c` | gemeinsamer I²C-Strang |
| `pins.h` | erzeugt, siehe oben |
