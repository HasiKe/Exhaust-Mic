# Gehäuse


| Teil | Maße | Volumen |
|---|---|---|
| `hauptgehaeuse-unterschale` | 94,0 × 73,4 × 16,6 mm | 34,6 cm³ |
| `hauptgehaeuse-deckel` | 89,0 × 73,4 × 8,7 mm | 16,4 cm³ |
| `hauptgehaeuse-serviceklappe` | 2,0 × 43,0 × 9,6 mm | 0,8 cm³ |
| `mikrofonkopf-unterschale` | 23,0 × 39,0 × 5,4 mm | 1,9 cm³ |
| `mikrofonkopf-oberschale` | 23,0 × 39,0 × 7,0 mm | 2,2 cm³ |


### Was das Hauptgehäuse groß macht

72 × 55 mm Platine, aber 94 × 73 mm außen. Drei Posten:

| Posten | Kostet |
|---|---|
| Antennenüberstand des WROOM-1 nach links | 6,55 mm |
| Kabelkanal vor den Steckern für die Hutmuttern | 8,00 mm |
| 5 mm Wand, zweimal je Achse | 10,00 mm |
| Verdickung für die Serviceklappe rechts | 5,00 mm |

Die Wand ist mit 5 mm dicker als üblich, weil die Dichtnut 2,6 mm breit
ist und beidseitig 1,2 mm Steg braucht. Ein dünnerer Rand mit angesetztem
Flansch wäre leichter, aber der Absatz nach außen ist beim Drucken ein
Überhang. Der Kabelkanal ist nicht verzichtbar: die Steckergehäuse
beginnen 0,8 mm hinter der Platinenkante, die Hutmuttern der
Verschraubungen brauchen davor Platz.

### Dichtheit ohne Durchbrüche

- **Taster**: über jedem Taster steht der Deckel nur 0,7 mm stark, darunter
  ein Stempel Ø4,0 bis 0,3 mm über die Tasterkappe. Die Membran federt, die
  Dichtung bleibt geschlossen. Schaltweg des TL3342 ist 0,25 mm — das
  schafft eine 0,7-mm-Membran über Ø9,0. Ein Durchbruch mit Stößel und
  O-Ring wäre präziser, aber vier weitere Leckstellen.
- **Leuchtdioden**: 0,6 mm Restwand statt Loch. Nur mit lichtdurchlässigem
  Filament sinnvoll; sonst die Tasche durchbohren und Lichtleiter einsetzen.
- **USB-C und microSD** liegen hinter einer gemeinsamen Serviceklappe mit
  eigener Dichtschnur. Die Nase der USB-Buchse ragt ohnehin 3 mm in die
  Wand hinein.
- **Dichtung**: Rundschnur Ø2,0 mm, rund 290 mm Umfang, abgelängt und
  stumpf geklebt. Für diesen Umfang gibt es keinen O-Ring von der Stange.

### Der Mikrofonkopf verträgt kein PLA und kein PETG

⚠️ **Das Mikrofon ist auf 85 °C begrenzt.** Für Kopf B in der Airbox ist
das unkritisch. Kopf A am Endrohr braucht ein Filament, das mindestens so
viel aushält wie das Mikrofon:

| Filament | Formbeständig bis etwa | Kopf A |
|---|---|---|
| PLA | 55 °C | nein |
| PETG | 75 °C | nein |
| ASA / ABS | 95 °C | brauchbar, UV-fest |
| PC, PA-CF | 110 °C und mehr | besser |

Das Gehäuse ist damit nicht das begrenzende Teil, aber es darf auch nicht
das schwächste sein. Für die Hauptplatine am Heck ebenfalls ASA: PETG
kreidet unter UV.

Offen bleibt O4 aus der Spezifikation — die zulässige Einbauposition von
Kopf A muss vor der Endmontage mit einem Thermoelement ausgemessen werden.
Hitzeschild und Abstandshalter sind **nicht** Teil dieser Lieferung.


Die Dichtheit macht das Gehäuse, nicht die Platinensteckverbinder: Superseal
1.0 gibt es nur mit 26, 34 oder 60 Wegen als Platinenheader, die kleinen 5-
und 6-poligen sind reine Kabelsteckverbinder. Auf der Platine sitzen deshalb
JST XH, und die Superseal-Trennstellen wandern ins Kabel.

Vorgesehen ist ein IP67-Kunststoffkasten von etwa 80 × 62 × 25 mm mit
Kabelverschraubungen. Zwei Punkte sind dabei bindend:

- Der Mikrofonkopf hat **keine Befestigungsbohrungen und keine
  Zugentlastung** mehr. Beides muss das Röhrchen übernehmen: Platine
  klemmen oder vergießen, und den Kabelzug abfangen, bevor er an den
  Lötstellen zieht. Rund um die Schallbohrung bleibt die Rückseite bis
  y 5,7 mm frei — dort dichtet das Röhrchen ab; Bauteile stehen erst
  darüber.
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
