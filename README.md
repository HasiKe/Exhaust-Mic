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

Ändert sich mit dem Bauteil auch das Landmuster, tauscht
`tools/swap_footprint.py` es auf der Platine und ordnet die Netze **über die
Padnamen** aus der Netzliste zu — die Reihenfolge stimmt zwischen zwei
Bibliotheken nie. Danach räumt `fix_drc.py` das Kupfer weg, das dem neuen
Landmuster im Weg liegt, und `tools/trim_warnings.py` entfernt lose
Bahnenden und zu dicht gesetzte Bohrungen. Letzteres geht einzeln vor: was
der Prüfer „lose" nennt, kann nach dem Füllen der Flächen doch tragen, und
pauschales Löschen hat die Zahl der offenen Verbindungen schon einmal
erhöht.

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

`pipeline.sh` führt der Reihe nach aus: Bestückung, Autorouting, Massefläche
vernähen, Restverbindungen, beanstandetes Kupfer entfernen, Restverbindungen
noch einmal, Versorgung aufweiten, Aufräumen, Regelprüfung. Die beiden
Nachbesserungsläufe sind kein Versehen: `fix_drc.py` schneidet Kupfer heraus
und reißt dabei Verbindungen auf, die der zweite Durchgang zurückholt. Die
Einzelschritte lassen sich auch getrennt aufrufen:

| Schritt | Was er tut |
|---|---|
| `build_pcb.py` | Kontur, Bestückung, Kupferflächen |
| `route.py` | Autorouting über Specctra-DSN und freerouting |
| `stitch.py` | Durchkontaktierung von Pads auf ihre Versorgungsfläche |
| `handroute.py` | einfache Wege für den Rest, sonst Via auf die Fläche |
| `widen_supply.py` | Versorgungsleitungen aufweiten, soweit der Platz reicht |
| `trim_warnings.py` | lose Bahnenden und zu dichte Bohrungen einzeln entfernen |
| `swap_footprint.py` | Landmuster tauschen, Netze über die Padnamen zuordnen |
| `cleanup.py` | doppelte Bohrungen, Nullbahnen, lose Enden entfernen — mit Nachzählen, siehe unten |
| `verify.py` | eigene Abnahme (Kontur, Durchgang, Abstände) |

### Kupfer entfernen ist gefährlich

Drei Werkzeuge nehmen Kupfer weg — `fix_drc.py`, `cleanup.py`,
`trim_warnings.py`. Alle drei können dabei Verbindungen aufreißen, und zwar
aus demselben Grund: **ein Segment, das nirgends an einem Pad hängt, kann
trotzdem tragen.** Läuft es quer durch eine gefüllte Massefläche, verbindet
es deren Stücke miteinander. Der Test „beide Enden frei in der Luft" sieht
das nicht.

Pauschal entfernt hat das die offenen Verbindungen von 7 auf 9 gehoben.
`cleanup.py` zählt deshalb nach: wird es nach dem Entfernen schlechter,
kommt alles zurück und jedes Segment wird einzeln probiert. `fix_drc.py`
zerstört weiterhin ein bis zwei Verbindungen je Lauf — dafür laufen die
Nachbesserungen ein zweites Mal.

Ebenso setzten `handroute.py` und `stitch.py` ihre Durchkontaktierungen
früher ohne Blick auf die Kontur. Zwei Nähvias landeten 0,7 mm neben dem
Anker der Massefläche bei (0,3 / 0,3) und damit **außerhalb der Platine** —
Kupfer, das die Fräse wegnimmt. `pcbgen.inside_outline()` prüft jetzt acht
Punkte auf dem Rand des Vias gegen das Konturpolygon; ein Mittelpunkttest
allein übersieht die abgerundeten Ecken.

### Steckverbinder ausrichten

Bei `rot=0` zeigt die Öffnung eines Kantensteckers auf **+y**, nicht auf −y.
Die Annahme war einmal andersherum, und dadurch zeigten J1 bis J4 mit der
Öffnung ins Platineninnere; beim USB-C-Anschluss war die Buchse damit gar
nicht erreichbar. Der Footprint sagt es selbst:

| Footprint | Woran man es sieht |
|---|---|
| `JST_XH_S*B-XH-A_..._Horizontal` | Der F.Fab-Umriss hat zwischen den beiden Seitenohren eine Aussparung von y −2,3 bis +2,2 — dort treten die Stifte nach unten aus. Die Kontaktkanäle laufen von y 3,2 bis 8,7 nach vorn, die Frontfläche liegt bei y = +9,2. |
| `USB_C_Receptacle_GCT_USB4085` | Eine Linie auf `Dwgs.User` mit der Aufschrift **PCB Edge** bei y = +6,1. Dort muss die Platinenkante liegen, die Nase ragt bis y = +8,61 darüber hinaus. |
| `microSD_HC_Hirose_DM3D-SF` | Der Kartenumriss auf F.Fab reicht bis y = +10,08, der Halterkörper endet bei +5,73. Die Karte steht also 4,35 mm heraus — das Kollisionsrechteck von `place()` muss diesen Kanal enthalten, sonst stellt `pack()` etwas darunter. |

Für die Kante gilt damit:

| Kante | Drehung |
|---|---|
| oben (−y) | 180 |
| rechts (+x) | 90 |
| unten (+y) | 0 |
| links (−x) | 270 |

Vor jeder Platzierung eines Kantensteckers die Marke oder die Aussparung im
Footprint nachsehen, nicht raten.

### Leiterbahnbreiten

freerouting verlegt alles in der Vorgabebreite von 0,18 mm. Für Signale ist
das richtig, für den Versorgungspfad nicht: 0,18 mm bei 35 µm Außenkupfer
tragen nach IPC-2221 rund 0,5 A bei 10 K Erwärmung, der ESP32 zieht in
Sendespitzen annähernd 0,5 A allein. `widen_supply.py` weitet deshalb im
Nachgang auf — gestaffelt und gruppenweise, die Gruppe mit dem größten Strom
zuerst, damit sie den Platz bekommt:

| Gruppe | Ziel |
|---|---|
| Schaltknoten, Buck-Ausgang, +4V6, +3V3D | 0,5 mm |
| Bordnetzeingang, VBAT12, USB_VBUS, GND | 0,4 mm |
| +3V3A | 0,3 mm |

Zurückgenommen wird nach **KiCads** Regelprüfung, nicht nach der Näherung in
`pcbgen.py`: die kennt die tatsächlichen Padformen nicht und lässt Bahnen
durchgehen, die an einer Kühlfahne zu nah liegen. Zwei Fallen dabei, beide
teuer bezahlt:

* Nur auf `clearance` zu filtern reicht nicht. Eine aufgeweitete Bahn kann
  ein fremdes Pad überdecken, und das meldet KiCad als **Kurzschluss** oder
  als Brücke im Lötstopplack. Beim ersten Versuch blieben so fünf echte
  Kurzschlüsse stehen, während das Werkzeug „keine Abstandsfehler mehr"
  meldete.
* Je Verstoß wird höchstens **eine** Bahn verschmälert, die breitere der
  beteiligten. Nimmt man beide zurück, verliert man an jeder engen Stelle
  doppelt.

Was danach noch bei 0,18 mm liegt, lässt sich ohne neues Routing nicht
verbreitern — der Nachbar steht im Weg. Der auffälligste Rest ist eine
24 mm lange USB_VBUS-Strecke auf In2 zwischen zwei SD-Datenleitungen: bei
0,5 A aus dem USB-Anschluss rund 23 K Erwärmung. Das passiert nur am
Schreibtisch, nicht auf der Fahrt.

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
- Die microSD-Karte sitzt so weit innen, dass ihre Spitze im
  eingeschobenen Zustand 0,5 mm **vor** der Platinenkante endet. Sie steht
  also nicht über und braucht keine Aussparung, wohl aber einen Schlitz im
  Deckel, durch den man sie einführt. Für den laufenden Betrieb ist der
  Download über WLAN vorgesehen.
- ⚠️ **Die USB-C-Buchse ragt 2,51 mm über die rechte Kante — das gehört so
  und darf nicht „korrigiert" werden.** Der GCT USB4085 ist laut Datenblatt
  eine *Dip Type, PCB Top Mount*; im *Recommended PCB Layout* auf Blatt 2
  zeichnet GCT eine durchgezogene Linie als Platinenkante und lässt den
  Bauteilumriss darüber hinausragen. KiCad hat diese Linie als `PCB Edge`
  auf Dwgs.User übernommen, bei lokal y = +6,1. Grund: die Buchse steht nur
  rund 3,5 mm über der Platine, die Umspritzung eines USB-C-Steckers ist
  aber etwa 6,5 mm hoch und mittig auf der Steckachse — ihre untere Hälfte
  liegt unterhalb der Platinenoberfläche. Reicht die Platine dort hin, stößt
  der Stecker gegen die Kante, bevor er einrastet. Das Gehäuse braucht an
  dieser Stelle ohnehin eine Öffnung; die Buchse ragt in sie hinein.
  Wer den Überstand wirklich loswerden will, braucht eine andere Bauform
  (senkrechte Buchse oder eine bündig aufsetzende SMD-Variante), nicht eine
  andere Position.

Anordnung der Bedienteile:

| Wo | Was |
|---|---|
| obere Kante | Mikrofon A, Mikrofon B, Bordnetz — drei JST XH nebeneinander, Kabel gehen alle in dieselbe Richtung ab |
| rechte Kante | USB-C oben, microSD darunter |
| untere Kante | drei Leuchtdioden als Säule, daneben die vier Taster in einer Reihe |
| Rückseite | nur flache Bauteile, nichts über 2,7 mm |

⚠️ Damit liegt das Bordnetzkabel mit 12 V, CAN und dem geschalteten Strom
des Buck-Wandlers **1,5 mm neben den Mikrofonleitungen**. Auf der Platine
ist das beherrscht — Gleichtaktdrosseln und Klemmdioden sitzen direkt hinter
J2 und J3, und der Regler steht am anderen Ende —, im Kabelbaum aber nicht:
dort gehören die Mikrofonleitungen geschirmt und getrennt vom Versorgungs-
strang geführt. J1 hat sechs Wege, J2 und J3 fünf; vertauschen kann man sie
nicht. J2 und J3 sind untereinander gleich, ein Tausch dreht nur die Kanäle.

Bauhöhe: Oberseite 7,6 mm (JST XH), Rückseite 2,7 mm (DS3231 im SOIC-16W).
Mit 1,6 mm Platine sind das **11,9 mm** Gesamtaufbau. Vorher saß die
Knopfzelle mit 4,3 mm hinten, das waren 13,5 mm. Die beiden stehenden
Stiftleisten J6 und J7 ragen 8,5 mm heraus und sind damit das höchste auf
der Platine; J6 (GPS) ist unbestückt, J7 (I2C-Erweiterung) lässt sich bei
Platznot durch eine liegende Bauform ersetzen.

## Stand

| | Mikrofonkopf | Hauptplatine |
|---|---|---|
| Bauteile | 14 | 127 |
| Leiterbahnen | 116 (259 mm) | 1047 (3236 mm) |
| Durchkontaktierungen | 3 | 919 |
| Offene Verbindungen | **0** | **7** |
| DRC-Fehler | **0** | **0** |
| DRC-Warnungen | 0 | 5 (Siebdruck, ein loses Via) |
| ERC-Fehler | **0** | **0** |
| Schaltplanparität | 2 (Bohrungen) | 4 (Bohrungen) |
| Leiterbahnbreiten | 0,18 mm | 0,18 mm Signale, 0,25–0,5 mm Versorgung |
| Bauhöhe | — | 7,6 mm oben, 2,7 mm unten |

Der Mikrofonkopf ist fertig. Auf der Hauptplatine bleiben **7 Verbindungen
offen**: vier Massefläche-Stücke auf der Rückseite und drei Pads
(U5 Pin 7 und C34 an Masse, R47 an +3V3D). Kein Signalnetz ist betroffen.
Sie sind in KiCads interaktivem Router (Push-and-Shove) in wenigen Minuten
zu schließen; der ist den hier verwendeten Werkzeugen an engen Stellen
deutlich überlegen. Vor der Fertigung müssen sie geschlossen sein.

Vorher waren es fünf. Die Oberseite trägt seit dem Umbau die Knopfzelle,
die vier Taster und den Piezo zusätzlich — rund 590 mm² mehr Sperrfläche —,
und die Masseflächen auf den Außenlagen zerfallen entsprechend in mehr
Stücke. Der Autorouter streut dabei stark: sechs Läufe mit identischer
Bestückung endeten zwischen 7 und 15 offenen Verbindungen. Es lohnt sich,
`pipeline.sh` zwei- bis dreimal laufen zu lassen und das beste Ergebnis zu
behalten.

Beschaffung: von 75 Stücklistenpositionen brauchen drei kein Bauteil (TP1,
TP2, Lötpads am Mikrofonkopf), 71 haben eine bei DigiKey lagernde
Herstellernummer — Stand 31.08.2026, siehe `output/beschaffung-digikey.csv`;
rund 65 EUR Bauteilkosten für die Hauptplatine und 4 EUR für den
Mikrofonkopf. Elf Positionen weichen auf einen gleichwertigen Ersatz aus,
weil der eingetragene Typ leer war; bei fünf davon ist auch der `Value`
nachgezogen (U2 → NCP1117ST33, U3 → LP5907MFX-3.3, U8 → SN65HVD230Q,
Q1 → SI2309CDS, U1 am Mikrofonkopf → LP5907MFX-2.8). Der Grund steht jeweils
im Feld `Hinweis` des Bauteils und damit auch in der Stückliste.

Bestände altern schnell: PCM1863DBTR und TPS7A2028PDBVR waren am Vormittag
noch lagernd und am Abend leer. Beide sind auf gleichwertige Typen umgestellt
(PCM1863**Q**DBTR**Q1** in Automotive-Qualifizierung, LP5907MFX-2.8). Vor
einer Bestellung lohnt ein erneuter Lauf von `pick_digikey.py`.

Eine Position bleibt offen:

| Position | Grund |
|---|---|
| MK1 | IM73A135V01XTSA1 ist aktiv, aber bei DigiKey ohne Bestand; ein Ersatz mit gleichem Gehäuse und Schallöffnung von unten existiert nicht. Andere Distributoren führen ihn. |

### Schaltplan gegen Platine

Die Footprints im Schaltplan sind auf den Stand des Layouts gezogen: U5 auf
`TSSOP-30_4.4x7.8mm_P0.5mm` (so steht es im Datenblatt), U6 auf das
S3-Modul, L2/L3 auf `L_CommonMode_Wuerth_WE-SL2`, J1–J3 auf JST XH
(Superseal 1.0 sitzt im Kabelbaum, es gibt sie nicht als Platinenversion),
BT1 auf den 12-mm-Halter und LS1 auf den 9 × 9 mm großen SMD-Piezo. Wo sich
damit das Bauteil ändert, sind Wert und Bestellnummer mitgezogen — BT1 ist
jetzt **CR1220** statt CR2032, also rund ein Viertel der Kapazität für die
Uhr-Pufferung.

Umgekehrt ist **J4** dem Bauteil gefolgt: den XKB-Typ führt DigiKey nicht,
also sitzt dort jetzt der GCT USB4085-GF-A und im Layout dessen Landmuster.
Das ist kein Tausch gleicher Bauform — der XKB klebt mit einer Padreihe
oben auf, der GCT steckt mit sechzehn Anschlüssen in zwei versetzten Reihen
durch die Platine. Mechanisch ist das am Motorrad die bessere Wahl; dafür
sperren die Bohrungen alle vier Lagen, und das Layout musste an der Stelle
neu. Auf der Rückseite ragen die Stifte heraus — das Gehäuse braucht dort
Luft. Der Autorouter ist danach einmal komplett durchgelaufen und kam auf
**fünf** offene Verbindungen statt der sieben vorher.

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
