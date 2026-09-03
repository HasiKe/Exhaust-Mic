# Beschaffung


Die Bauteilfelder **MPN**, **Hersteller** und **DigiKey** stehen in den
Schaltplänen und wandern von dort in die Stückliste. Sie werden nicht von
Hand gepflegt, sondern gegen DigiKeys Produktdatenbank ermittelt — mit
Lagerfilter, damit nichts in der Stückliste steht, was man nicht bestellen
kann.

> Die `tools/`-Skripte liegen **nicht im Repository**. Der Ablauf ist hier
> beschrieben, seine Ergebnisse stehen in den Schaltplänen und unter
> `output/`.

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

## Stand der Bestellliste

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
