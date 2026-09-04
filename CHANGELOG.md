# Änderungen

Alle nennenswerten Änderungen an diesem Projekt stehen hier.
Das Format folgt [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionierung [Semantic Versioning](https://semver.org/lang/de/).

## [0.1.0] — 2026-09-04

Erste Veröffentlichung. Schaltplan, Layout, Gehäuse und Firmware sind
vollständig, **auf Hardware gelaufen ist davon nichts** — es gibt noch
keinen Aufbau.

### Hardware

- Hauptplatine, 72 × 55 mm, vier Lagen, beidseitig bestückt, 131 Bauteile.
  Sechs Schaltplanblätter: Versorgung, Analogteil, Wandler, Rechner, Ein-
  und Ausgabe.
- Mikrofonkopf, 8 × 27 mm, zwei Lagen, 14 Bauteile, zweimal identisch
  aufzubauen. Eigenes KiCad-Projekt unter `mic-head/`.
- Zwei differentielle Kanäle mit 135 dBSPL Grenzschalldruck: IM73A135 auf
  OPA2325 als Impedanzwandler, 2,8-V-LDO im Kopf, vier Meter geschirmtes
  Twisted Pair zur Hauptplatine, dort Gleichtaktdrossel und Klemmdioden.
- PCM1863 als Wandler, 24 Bit bei 48 kHz, 110 dB Störabstand, Taktherr an
  einem eigenen 24,576-MHz-Oszillator; der ESP32-S3 empfängt nur.
- ESP32-S3-WROOM-1-N8R2 mit microSD-Karte, DS3231-TCXO (±2 ppm) als
  Zeitbasis, CAN über SN65HVD230.
- Versorgung aus dem 12-V-Bordnetz über einen LM5164-Abwärtswandler mit
  Verpolschutz.
- **Zündungserkennung.** Die Freigabe des Wandlers hängt an Klemme 15, die
  Versorgung an Klemme 30. Zündung aus schaltet nicht sofort stromlos: der
  ESP32 hält seine eigene Versorgung über `PWR_HOLD` und D8 fest, schließt
  die Datei und lässt dann los. Ruhestrom rechnerisch 30 µA statt der rund
  100 mA einer dauernd laufenden Platine.
- Gehäuse aus fünf gedruckten Teilen mit Rundschnurdichtung, parametrisch
  aus den Platinendateien erzeugt. Kopf A sitzt am Endrohr — das Mikrofon
  ist auf 85 °C begrenzt, deshalb ASA aufwärts.
- Alle Kabelstecker an der oberen Kante nach außen, USB-C und microSD
  rechts hinter einer gemeinsamen Serviceklappe, Leuchtdioden und Taster
  unten. Aufbauhöhe 11,9 mm, auf der Rückseite steht nichts über 2,7 mm.

### Firmware

ESP-IDF 5.3.2, Ziel `esp32s3`, baut warnungsfrei, 664 kB Abbild.

- F1 I2S-Slave-Empfang per DMA (`audio.c`)
- F2 vorallokierte Datei, Kopf beim Schließen gepatcht (`speicher.c`)
- F3 BWF/BEXT-Zeitstempel aus der RTC (`speicher.c`)
- F4 1-Hz-Anker latcht den Samplezähler (`zeitbasis.c`) — driftfreie
  Zuordnung Wanduhrzeit ↔ Sample über die ganze Aufnahme
- F5 PCM1863 über I²C, differentiell, Taktherr (`pcm1863.c`)
- F6 Übersteuerungsanzeige (`bedienung.c`)
- F7 Zündungsautomat mit sauberem Herunterfahren (`zuendung.c`)
- F8 Sync-Marker aus Blitz und Piep samt Samplenummer (`bedienung.c`)
- GoPro-Kopplung über Bluetooth LE nach der Open-GoPro-Schnittstelle
  (`gopro.c`): Video starten heißt Aufnahme starten, der Taster am Gerät
  löst umgekehrt die Kamera aus.
- **Der Motor ist der dritte Auslöser** neben Taster und Kamera: springt er
  an, hebt die Lichtmaschine das Bordnetz über 13,2 V.
- `pins.h` wird aus der Netzliste erzeugt, nicht abgetippt
  (`firmware/tools/pins_aus_netzliste.py`).

### Fertigungsunterlagen

Unter `output/`: Gerber-Pakete beider Platinen, Positionsdateien,
Stücklisten, Schaltplan- und Layout-PDF, Bilder. 71 von 76
Beschaffungspositionen tragen eine bei DigiKey lagernde Herstellernummer.

### Geprüft

DRC, ERC, Schaltplanparität, eine eigene Abnahme und der Übersetzungslauf
der Firmware.

| | Mikrofonkopf | Hauptplatine |
|---|---|---|
| Offene Verbindungen | 0 | 8 |
| DRC-Fehler | 0 | 0 |
| ERC-Fehler | 0 | 0 |
| Schaltplanparität | 0 | 6 |

### Bekannte Einschränkungen

- **Kein Aufbau, keine Messung.** Alles hier ist gerechnet, geprüft und
  übersetzt — nichts ist gelötet.
- Auf der Hauptplatine bleiben **acht Verbindungen offen**, allesamt Stücke
  der Masseflächen, kein Pad und kein Signalnetz. Vor der Fertigung im
  interaktiven Router zu schließen.
- **R2 (680 k)** hat beim Umbau auf die Zündungsfreigabe seinen Wert
  gewechselt und trägt noch keine geprüfte Bestellnummer.
- MK1, das IM73A135V01XTSA1, ist aktiv, bei DigiKey aber ohne Bestand;
  andere Distributoren führen es.
- Die sechs Abweichungen der Schaltplanparität sind die vier
  Befestigungsbohrungen ohne Symbol und die Schirmanschlüsse von USB-C und
  Kartenhalter, die der Footprint führt und das Symbol nicht.
- Offen aus der Spezifikation: F6 automatische Pegelabsenkung, F9 WLAN und
  NTP, F10 CAN in die Begleitdatei, F11 Download über USB-MSC oder WLAN.
- Die Platine kann der Kamera **keinen Ton liefern**. Der ESP32-S3
  beherrscht nur Bluetooth LE, GoPro nimmt drahtlosen Ton über HFP an —
  ein Bluetooth-Classic-Profil, und einkanalig mit 8 oder 16 kHz.
- Die Generatorskripte (`tools/`, `case/`) liegen nicht im Repository. Die
  Dateien auf der Platte sind weiter als die Generatoren; ein Neulauf würde
  Beschaffungsfelder, korrigierte Landmuster, zwei reparierte Symbole und
  fünf PWR_FLAG überschreiben. Siehe `docs/entwicklung.md`.

[0.1.0]: https://github.com/HasiKe/Exhaust-Mic/releases/tag/v0.1.0
