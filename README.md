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

> **Achtung:** Die Dateien auf der Platte sind inzwischen weiter als die
> Generatoren. `build_main.py` und `build_mic_head.py` überschreiben sie
> vollständig — unter anderem gehen die Beschaffungsfelder und die von Hand
> nachgezogenen Änderungen verloren. Vor einem Lauf den Stand sichern und
> danach `tools/apply_digikey.py` erneut ausführen.

`build_main.py` meldet beim Lauf jede Leitung, die über einen Bauteilpin
hinwegläuft — solche Stellen sind fast immer versehentliche Kurzschlüsse und
im gedruckten Plan nicht zu sehen. Ein sauberer Lauf gibt keine Warnungen aus.

## Beschaffung

Die Bauteilfelder **MPN**, **Hersteller** und **DigiKey** stehen in den
Schaltplänen und wandern von dort in die Stückliste. Sie werden nicht von
Hand gepflegt, sondern gegen DigiKeys Produktdatenbank ermittelt — mit
Lagerfilter, damit nichts in der Stückliste steht, was man nicht bestellen
kann.

```sh
set -a; . ~/.config/secrets.env; set +a     # DIGIKEY_CLIENT_ID / _SECRET
kicad-cli sch export bom \
    --fields 'Reference,Value,Footprint,MPN,${QUANTITY}' \
    --labels 'Referenzen,Wert,Footprint,MPN,Menge' \
    --group-by 'Value,Footprint,MPN' -o /tmp/grp-main.csv exhaust-mic.kicad_sch
python3 tools/pick_digikey.py /tmp/grp-main.csv /tmp/grp-mic.csv \
        -o tools/digikey-wahl.json
python3 tools/apply_digikey.py tools/digikey-wahl.json
python3 tools/build_bom.py output/main.net output/bom-mainboard.csv
```

`pick_digikey.py` prüft jeden Treffer gegen die Parameter, die DigiKey selbst
mitliefert — Kapazität, Spannungsfestigkeit, Dielektrikum, Widerstandswert,
Toleranz, Bauform. Die Stichwortsuche allein reicht nicht: auf „10 kOhm 1 %
0603“ antwortet DigiKey auch mit 11,5 k in 0201. Was aus der Schaltung kommt
und nicht in der Stückliste steht (100 V am Bordnetzeingang, C0G im Filter,
1 % am Reglerteiler), steht in der Tabelle `SPEC`; Mechanik und
Steckverbinder in `MANUELL`; nicht lagernde Typen mit ihrem Ersatz und der
Begründung in `ERSATZ`.

`apply_digikey.py` schreibt die Felder in die **vorhandenen** Schaltplandateien
— es erzeugt nichts neu. Vorhandene Hinweise bleiben stehen und werden
ergänzt, nicht überschrieben.

`output/beschaffung-digikey.csv` hält Bestand, Einzelpreis und Ersatzgrund zum
Abfragezeitpunkt fest. Preise und Bestände altern; die Datei ist ein Beleg,
keine laufende Quelle.

## Platinen neu erzeugen

Das Layout entsteht auf demselben Weg: `pcbgen.py` kapselt die pcbnew-API,
die beiden `build_*_pcb.py` beschreiben Kontur, Bestückung und Kupferflächen,
`route.py` verlegt die Leiterbahnen mit freerouting.

```sh
kicad-cli sch export netlist --output output/main.net exhaust-mic.kicad_sch
tools/pipeline.sh <arbeitsverzeichnis>              # alles in einem Lauf
python3 tools/build_outputs.py                      # Gerber, Bohrdaten, PDF
```

`pipeline.sh` führt der Reihe nach aus: Bestückung, Autorouting,
Restverbindungen, Aufräumen, Regelprüfung. Die Einzelschritte lassen sich
auch getrennt aufrufen:

| Schritt | Was er tut |
|---|---|
| `build_pcb.py` | Kontur, Bestückung, Kupferflächen |
| `route.py` | Autorouting über Specctra-DSN und freerouting |
| `stitch.py` | Durchkontaktierung von Pads auf ihre Versorgungsfläche |
| `handroute.py` | einfache Wege für den Rest, sonst Via auf die Fläche |
| `cleanup.py` | doppelte Bohrungen, Nullbahnen, lose Enden entfernen |
| `verify.py` | eigene Abnahme (Kontur, Durchgang, Abstände) |

`stitch.py` und `handroute.py` nehmen die offenen Verbindungen aus
`kicad-cli pcb drc` statt aus eigener Vermutung und lehnen jeden Weg ab,
der die Abstandsregel verletzt. Bei `handroute.py` ist die Prüfung auf
gleiche Netznamen nicht optional: KiCad nennt in „Missing connection
between items" gelegentlich ein zweites Element aus einem fremden Netz —
ohne die Sicherung entsteht daraus ein Kurzschluss.

Seit KiCad 10 kommen die eingebauten Prüfungen dazu — die gab es in
Version 7 noch nicht:

```sh
kicad-cli sch erc --severity-error exhaust-mic.kicad_sch
kicad-cli pcb drc --severity-error --schematic-parity exhaust-mic.kicad_pcb
```

`build_pcb.py --map` zeichnet eine grobe ASCII-Belegungskarte je Seite —
damit lässt sich die Aufteilung beurteilen, ohne KiCad zu öffnen.

Große und mechanisch gebundene Teile stehen im Bestückungsplan fest, alles
andere sortiert `pack()` regalweise in benannte Regionen. Das überlebt
Änderungen an einzelnen Footprints, ohne dass 127 Koordinaten nachgeführt
werden müssen.

Für `route.py` muss `fr-1.9.0.jar` (freerouting) im Arbeitsverzeichnis
liegen, und `xvfb-run` muss installiert sein — freerouting baut auch im
Stapelbetrieb eine Oberfläche auf und bricht ohne Display ab.

Bewusst **nicht** die neuere v2.1.0: die schreibt die SES-Datei erst am
Ende des Laufs, endet aber nur, wenn sie auf null offene Verbindungen
kommt. Bleibt ein Rest, dreht sie endlos — `max_passes`, `stop_pass_no`,
`job_timeout` und `optimizer.max_passes` werden allesamt ignoriert
(beobachtet bis Durchgang 434). Ein Abbruch von außen verwirft dann das
komplette Routing. v1.9.0 hält sich an `-mp`.

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

## Bestückungsdruck

Der Siebdruck trägt nur die Steckverbinder-Bezeichner. Alles andere —
Referenzen der Kleinteile, Werte, Herstellernummern — liegt auf der
Fab-Lage: zwischen zwei 0603-Bauteilen sind 0,5 mm, ein lesbarer Bezeichner
passt dort nicht hin, ohne das Nachbarpad zu überdrucken. Für die
Bestückung ist deshalb die Fab-Lage im PDF maßgeblich.

Beschaffungsfelder werden beim Platzieren automatisch unsichtbar gesetzt
und auf die Fab-Lage gelegt. Ohne das landen sie sichtbar im Siebdruck und
überdecken die halbe Platine — das waren 151 der ursprünglich 214
Regelwarnungen.

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

| | Mikrofonkopf | Hauptplatine |
|---|---|---|
| Bauteile | 14 | 127 |
| Leiterbahnen | 116 (259 mm) | 1143 (3249 mm) |
| Durchkontaktierungen | 3 | 936 |
| Offene Verbindungen | **0** | **7** |
| DRC-Fehler | **0** | **0** |
| DRC-Warnungen | 0 | 10 |
| ERC-Fehler | **0** | **0** |
| Schaltplanparität | 2 (Bohrungen) | 4 (Bohrungen) |

Der Mikrofonkopf ist fertig. Auf der Hauptplatine bleiben **7 Verbindungen
offen** — überwiegend Massefläche-Stücke auf der Rückseite, die durch die
Leiterbahnen abgeschnitten wurden und deren Anbindung an die durchgehende
Fläche auf In1 der Autorouter nicht findet. Sie sind in KiCads interaktivem
Router (Push-and-Shove) in wenigen Minuten zu schließen; der ist
den hier verwendeten Werkzeugen an engen Stellen deutlich überlegen. Vor der
Fertigung müssen sie geschlossen sein.

Beschaffung: von 75 Stücklistenpositionen brauchen drei kein Bauteil (TP1,
TP2, Lötpads am Mikrofonkopf), 70 haben eine bei DigiKey lagernde
Herstellernummer — Stand 31.08.2026, siehe `output/beschaffung-digikey.csv`;
rund 64 EUR Bauteilkosten für die Hauptplatine und 4 EUR für den
Mikrofonkopf. Neun Positionen mussten auf einen gleichwertigen Ersatz
ausweichen, weil der eingetragene Typ leer war; bei vier davon wurde auch
der `Value` nachgezogen (U2 → NCP1117ST33, U3 → LP5907MFX-3.3,
U8 → SN65HVD230Q, Q1 → SI2309CDS). Der Grund steht jeweils im Feld
`Hinweis` des Bauteils und damit auch in der Stückliste. Zwei Positionen
sind offen:

| Position | Grund |
|---|---|
| J4 | den XKB-Typ zum Footprint `USB_C_Receptacle_XKB_U262-16XN-4BVC11` führt DigiKey nicht |
| MK1 | IM73A135V01XTSA1 ist aktiv, aber bei DigiKey ohne Bestand; ein Ersatz mit gleichem Gehäuse und Schallöffnung von unten existiert nicht |

### Schaltplan gegen Platine

Die Footprints im Schaltplan sind auf den Stand des Layouts gezogen: U5 auf
`TSSOP-30_4.4x7.8mm_P0.5mm` (so steht es im Datenblatt), U6 auf das
S3-Modul, L2/L3 auf `L_CommonMode_Wuerth_WE-SL2`, J1–J3 auf JST XH
(Superseal 1.0 sitzt im Kabelbaum, es gibt sie nicht als Platinenversion),
BT1 auf den 12-mm-Halter und LS1 auf den 9 × 9 mm großen SMD-Piezo. Wo sich
damit das Bauteil ändert, sind Wert und Bestellnummer mitgezogen — BT1 ist
jetzt **CR1220** statt CR2032, also rund ein Viertel der Kapazität für die
Uhr-Pufferung.

Die beiden Symbolfehler sind ebenfalls behoben (`tools/fix_symbols.py`):

* **U2** trug ein Symbol mit vier Pins. Das SOT-223 hat drei Anschlüsse und
  eine Kühlfahne, die innen an Pin 2 hängt — Pin 4 gab es nie. Er lag am
  selben Netz wie Pin 2, elektrisch also folgenlos, und ist samt seiner
  beiden Drähte und dem Verbindungspunkt heraus. Damit ist auch der
  ERC-Fehler „Stromausgang gegen Stromausgang" weg.
* **J5** verwendete `Micro_SD_Card_Det1`. Dieses Symbol kennt einen
  Schaltkontakt und legt den Schirm auf Pin 10. Der DM3D-SF hat zwei
  Schaltkontakte (Pad 9 und 10) und den Schirm auf Pad 11. Jetzt steht dort
  `Micro_SD_Card_Det2`; die gemeinsamen Pins liegen an denselben Stellen,
  der neue Pin 10 (DET_A) ist an Masse gelegt.

KiCads Abgleich meldet danach noch **4 Abweichungen** auf der Hauptplatine
und **2** auf dem Mikrofonkopf — ausschließlich die Befestigungsbohrungen
H1–H4 beziehungsweise H1–H2, die absichtlich kein Symbol im Schaltplan
haben.

Die fehlenden `PWR_FLAG` sind gesetzt (`tools/add_pwr_flags.py`): an **+4V6**,
an **AVDD** (U5 Pin 8) und an **VBAT** (U7 Pin 14) auf der Hauptplatine, am
Reglereingang und an Masse beim Mikrofonkopf. KiCad prüft, ob jedes
Versorgungsnetz von einem Ausgang gespeist wird; diese fünf Netze kommen aus
einer Ferritperle, aus der Knopfzelle oder über das Kabel herein, also aus
etwas, das KiCad nicht als Quelle erkennt. Das Flag sagt dem ERC, dass das
Absicht ist — an der Schaltung ändert es nichts.

**ERC ist damit auf beiden Platinen fehlerfrei.**

Offen sind außerdem Gehäuse und Firmware.
