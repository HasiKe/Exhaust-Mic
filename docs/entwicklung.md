# Entwicklungsnotizen

Was beim Bauen schiefging und warum die Werkzeuge so aussehen, wie sie
aussehen. Für alle, die an den Generatoren weiterarbeiten — oder
dieselben Fallen umgehen wollen.

> Die Generatoren selbst (`tools/`, `case/`) liegen **nicht im
> Repository** — die Dateipfade unten benennen sie, ohne dass sie hier
> mitgeliefert würden. Im Repository stehen ihre Ergebnisse.

## Inhalt

- [Die Zündung schaltet, nicht das Bordnetz](#die-zündung-schaltet-nicht-das-bordnetz)
- [Steckverbinder ausrichten](#steckverbinder-ausrichten)
- [Kupfer entfernen ist gefährlich](#kupfer-entfernen-ist-gefährlich)
- [Der Autorouter kennt den Kantenabstand nicht](#der-autorouter-kennt-den-kantenabstand-nicht)
- [Leiterbahnbreiten](#leiterbahnbreiten)
- [Der Mikrofonkopf ist auf ein Fünftel geschrumpft](#der-mikrofonkopf-ist-auf-ein-fünftel-geschrumpft)
- [Prüfen](#prüfen)
- [Schaltplan gegen Platine](#schaltplan-gegen-platine)
- [Bestückungsdruck](#bestückungsdruck)

---

## Die Zündung schaltet, nicht das Bordnetz

Bis Rev A hing `EN/UVLO` des LM5164 über R2/R3 fest an **VBAT12**, und
VBAT12 ist Klemme 30 — Dauerplus. Damit lief die Platine, sobald sie am
Fahrzeug hing: rund um die Uhr, mit dem Ruhestrom des NCP1117, dem
CAN-Treiber, dem Wandler und der Uhr an 3V3. Auf einem Motorradakku ist
das eine Frage von Wochen.

Jetzt speist **IGN_IN** (Klemme 15) den Teiler, und der ESP32 hält seine
eigene Versorgung fest, bis die Datei geschlossen ist:

```
IGN_IN ──[R2 680k]──┬── EN/UVLO (U1)
                    ├──[R3 150k]── GND        Einschaltschwelle
                    ├──[D7 BZX84C3V3]── GND   Klemme
                    │
PWR_HOLD ─┬─[R13 10k]──►|── D8 ───┘           Selbsthaltung
          └─[R14 10k]── GND
```

### Die Freigabeschwelle ist 1,5 V, nicht 1,2 V

Der naheliegende Fehler: 1,2 V aus dem Datenblatt ablesen und rechnen.
1,2 V ist die **FB-Referenz**, nicht die Freigabeschwelle. Für EN/UVLO
gilt (SNVSAU4D, Abschnitt 6.3.9, Gleichungen 13 und 14):

| | Wert | mit R2 680k / R3 150k | mit den alten 1M / 150k |
|---|---|---|---|
| Freigabe steigend | 1,5 V | **8,3 V** | 11,5 V |
| Freigabe fallend | 1,4 V | **7,8 V** | 10,7 V |

11,5 V wären für eine Zündleitung zu hoch: ein halb leerer Akku steht bei
11,8 V, und beim Anlassen bricht das Bordnetz ohnehin ein. 8,3 V liegen
unter jeder brauchbaren Zündspannung und über allem, was noch als „aus"
durchgeht. Unterhalb von 1,1 V an EN geht der Wandler in den Ruhezustand
und zieht **3 µA**.

### Warum eine Diode und ein Ableitwiderstand nötig sind

**D8** trennt den Portpin vom Teiler. Ohne die Diode zöge R13 den
EN-Knoten herunter, solange die Platine aus ist — der Portpin liegt dann
stromlos auf Masse:

    12 V × (150k ‖ 10k) / (680k + 150k ‖ 10k) = 0,16 V

Die Platine würde nie anlaufen. Mit Diode sperrt der Zweig, solange
PWR_HOLD nicht getrieben wird.

**R14** deckt den umgekehrten Fall ab. Der Rücksetzzustand von GPIO42 ist
nicht garantiert frei von einem internen Pullup; läge einer an, hielte
die Platine sich selbst fest und ginge nie wieder aus. 10 k gegen die
45 k des Pullups ergeben 0,6 V an der Anode — zu wenig, um D8 zu öffnen.

Getrieben reicht es dagegen deutlich, gerechnet mit 3,2 V Versorgung und
0,75 V Flussspannung im schlechtesten Fall:

    (3,2 − 0,75) V × (150k ‖ 680k) / (10k + 150k ‖ 680k) = 2,27 V

gegen 1,4 V Haltespannung.

R13 und R14 stehen im Layout beim Funkmodul. Der naheliegendere Platz für
R13 wäre neben D8 gewesen — dann läge der Knoten zwischen Widerstand und
Diode nicht 30 mm lang quer über die Platine. Das Band unter dem Regler
war damit aber überfüllt: dem Router fehlte der Ausweg, und **SD_CLK**
zwischen U6 und dem Kartenhalter blieb offen. Die Leitung führt eine
Gleichspannung aus einem Gegentaktausgang über eine durchgehende
Massefläche; 30 mm sind dafür ohne Belang, eine offene Taktleitung zur
Karte nicht.

### Kein Stützkondensator

Der Wandler hängt mit VIN weiter an Klemme 30. Geschaltet wird nur seine
Freigabe — die Energie steht während der ganzen Nachlaufzeit an. Eine
Selbsthaltung, die den Versorgungspfad selbst auftrennt, bräuchte einen
Puffer für die Sekunde, die das Schließen der Datei dauert; diese hier
nicht.

### D7 ist für die negativen Pulse da

Naheliegende Annahme: der Zener schützt EN gegen Überspannung. Das ist
nicht nötig — **EN verträgt −0,3 bis 100 V** (SNVSAU4D, Abschnitt 5.1),
und ein 60-V-Puls kommt über den Teiler nur auf 10,8 V.

Die untere Grenze ist das Problem. ISO 7637-2 kennt Pulse bis −100 V, und
bei −0,3 V ist am EN-Pin Schluss. In dieser Richtung leitet der Zener in
Flussrichtung und klemmt auf −0,7 V; der Strom über R2 bleibt bei 150 µA.
Für den Preis eines SOT-23, das ohnehin zweimal auf der Platine steckt.

### Ruhestrom

Bisher gab es kein Aus. Lag Bordspannung an, lief die Platine — und zwar
nicht im Leerlauf, sondern vollständig: ESP32 wach, Wandler getaktet,
CAN-Treiber aktiv, Karte eingehängt. Die Größenordnung liegt damit im
Bereich **100 mA**, nicht im Mikroampere-Bereich.

Jetzt bleibt bei abgezogener Zündung stehen:

| | Quelle | Strom |
|---|---|---|
| LM5164 im Ruhezustand | Datenblatt, Abschnitt 6.4.1 | 3 µA |
| Teiler `VBAT_SENSE` (470 k + 68 k) | 14,4 V / 538 kΩ | 27 µA |
| Teiler EN (jetzt an Klemme 15) | Klemme 15 ist spannungslos | 0 |
| TVS D2, Zener D1, PMOS Q1 | Sperrströme | wenige µA |
| **zusammen** | | **rund 30 µA** |

⚠️ Alle Werte sind **gerechnet**, nicht gemessen — es existiert kein
Aufbau. Der Punkt ist aber nicht die zweite Stelle hinter dem Komma: der
Unterschied zwischen „läuft durch" und „30 µA" ist der zwischen Tagen und
Jahren, bis der Akku leer ist.

Vorher hätte der EN-Teiler an VBAT12 selbst noch 13 µA gezogen
(14,4 V / 1,15 MΩ) — neben allem anderen war das der kleinste Posten.

### USB bleibt davon unberührt

`+4V6` bekommt über D4 auch Strom aus `USB_VBUS`. Am Schreibtisch läuft
die Platine also ohne Zündung weiter, und der Automat merkt das: er
schaltet nur ab, wenn er die Zündung vorher einmal gesehen hat. `PGOOD`
sagt zusätzlich, ob der Abwärtswandler regelt — der Pin hat **keinen
äußeren Pullup**, die Firmware schaltet den im Modul zu.

### Was beim Einbau schiefging

- **An eine `.kicad_sch` anhängen geht nicht.** Alles hinter der
  schließenden Klammer des Blattes liest KiCad wortlos nicht mehr. Die
  Datei öffnet sich, das ERC läuft, und die Netzliste meldet nur
  unverbundene Pins. Eingefügt wird vor `(sheet_instances`.
- **Ein leeres Feld ist nicht dasselbe wie kein Feld.** R2 bekam beim
  Wertwechsel ein leeres `DigiKey`-Feld. Leere Felder fallen aus der
  Netzliste heraus, und die Schaltplanparität meldete daraufhin
  „Fehlendes Symbolfeld DigiKey in Footprint". Das Feld muss ganz weg.
- **Die Regionen von `pack()` sind enger, als sie aussehen.** Drei Teile
  zusätzlich in „Buck-Umgebung" schoben C12 an eine Stelle, an die der
  Router nicht mehr herankam — der 100n am 3V3D-Zweig blieb offen. Das
  Band unter dem Regler beginnt deshalb jetzt bei x = 41 statt bei x = 43,
  und die drei Teile stehen woanders.
- **Feines Vernähen ist kein Gewinn.** Ein Rastermaß von 1,6 und 1,3 mm
  schloss zwei Massestücke, brachte aber 800 zusätzliche
  Durchkontaktierungen und 27 Regelfehler mit. Bei 2,0 mm ist Schluss.

## Steckverbinder ausrichten

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

## Kupfer entfernen ist gefährlich

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

## Der Autorouter kennt den Kantenabstand nicht

freerouting hält zur Begrenzung nur den allgemeinen Abstand ein — hier
0,15 mm. KiCad verlangt zur Platinenkante aber 0,25 mm
(`min_copper_edge_clearance`). Auf dem 8 mm schmalen Kopf hat der Router
deshalb drei Bahnen mit 0,23 mm an die rechte Kante gelegt: drei
Regelfehler, nachträglich nur durch Aufreißen der Bahn zu beheben.
`route.py` rückt die Begrenzung im DSN jetzt um 0,12 mm ein, bevor
freerouting startet — gerechnet auf den Koordinaten im DSN selbst, damit
offen bleibt, wo KiCad den Nullpunkt hingelegt hat.

## Leiterbahnbreiten

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

`route.py` braucht [freerouting](https://github.com/freerouting/freerouting)
**v1.9.0** als `fr-1.9.0.jar` im Arbeitsverzeichnis, dazu `xvfb-run` —
freerouting baut auch im Stapelbetrieb eine Oberfläche auf und bricht ohne
Display ab. Die JAR-Datei liegt bewusst nicht im Repository, sie ist
Fremdsoftware mit eigener Lizenz.

Bewusst **nicht** die neuere v2.1.0: die schreibt die SES-Datei erst am
Ende des Laufs, endet aber nur, wenn sie auf null offene Verbindungen
kommt. Bleibt ein Rest, dreht sie endlos — `max_passes`, `stop_pass_no`,
`job_timeout` und `optimizer.max_passes` werden allesamt ignoriert
(beobachtet bis Durchgang 434). Ein Abbruch von außen verwirft dann das
komplette Routing. v1.9.0 hält sich an `-mp`.

## Der Mikrofonkopf ist auf ein Fünftel geschrumpft

Von 22 × 40 mm auf **8 × 27 mm**, 880 mm² auf 216 mm². Dieselben vierzehn
Bauteile. Drei Dinge haben den Platz gekostet:

| Was | Ersparnis |
|---|---|
| Lötpad-Landmuster mit Zugentlastung, 20,05 × 13,27 mm | 228 mm² |
| Alles auf der Oberseite | rund die Hälfte der Restfläche |
| Zwei M2,2-Bohrungen mit 4 mm Pad an der Unterkante | rund 5 mm Länge |

Das Landmuster war der größte Posten: 266 mm² für einen einzigen Steckplatz,
mehr als die anderen dreizehn Bauteile zusammen (121 mm²). Fünf Pads
nebeneinander sind allein 20 mm breit und haben damit die Platinenbreite
diktiert. `lib/exhaust-mic.pretty/SolderWire-0.1sqmm_1x05_2reihig_P2.6mm`
setzt sie zweireihig versetzt auf 5,15 × 7,35 mm; die Drähte laufen jetzt
längs aus der Platine heraus statt quer, was für einen Kopf im Röhrchen
ohnehin die richtige Richtung ist.

Zwei Stolpersteine beim Umbau:

- **Die Durchsteckpads sperren die Rückseite auf ihrer ganzen Fläche.** Der
  erste Entwurf stellte die Kleinteile hinter die Kabelpads, `check()`
  meldete fünf Kollisionen. C1, C4 und FB1 sitzen deshalb vorn im freien
  Feld zwischen Wandler und Kabelpads.
- **Bei 8 mm Breite muss ein Kanal frei bleiben.** Erst blieb `OUT_P` als
  einziges Netz offen, weil der Regler U1 links im Weg stand — `OUT_N` kam
  rechts durch. Ab y 9,9 ist auf der Rückseite alles nach rechts gerückt;
  links bleiben 2,7 mm für die Bahn von R1 zu den Kabelpads.

## Prüfen

```sh
kicad-cli sch export netlist --output /tmp/exhaust-mic.net exhaust-mic.kicad_sch
kicad-cli sch export pdf     --output /tmp/exhaust-mic.pdf exhaust-mic.kicad_sch
python3 tools/build_bom.py /tmp/exhaust-mic.net output/bom-mainboard.csv
```

Die Netzliste ist die eigentliche Abnahme des Schaltplans: jedes Netz und
jeder offene Pin lassen sich gegen Abschnitt 7 und 10 der Spec abgleichen.
Aktuell bleiben 19 Pins bewusst offen (Strapping-Pins des ESP32, unbenutzte
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
