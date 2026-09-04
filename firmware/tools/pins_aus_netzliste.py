#!/usr/bin/env python3
"""firmware/main/pins.h aus der Netzliste erzeugen.

    python3 firmware/tools/pins_aus_netzliste.py [netzliste.net]

Die Pinbelegung steht im Schaltplan. Tippt man sie in die Firmware ab,
laeuft sie beim naechsten Schaltplanlauf auseinander - und ein vertauschter
GPIO faellt erst beim Aufbau auf. Der Header wird deshalb erzeugt und
mitversioniert, damit ein Diff sofort zeigt, wenn sich etwas verschoben
hat.

Die Nummer hinter dem Modulpin steht in der Pinfunktion: "IO18_11" heisst
Modulpin 11, GPIO18. Bei den beiden UART0-Pins heisst es RXD0/TXD0, das
sind GPIO44 und GPIO43.
"""

from __future__ import annotations

import os
import re
import sys

HIER = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

# Netz -> (Bezeichner, Kommentar). Was hier nicht steht, kommt nicht in
# den Header; GND und die Versorgungen braucht die Firmware nicht.
NETZE = {
    "I2S_BCK":     ("I2S_BCK", "Bittakt vom PCM1863, der ist Master"),
    "I2S_LRCK":    ("I2S_LRCK", "Wortakt vom PCM1863"),
    "I2S_DOUT":    ("I2S_DIN", "Daten vom Wandler, aus Sicht des ESP32 Eingang"),
    "PCM_INT":     ("PCM_INT", "Sammelmeldung des PCM1863, u. a. Uebersteuerung"),
    "I2C_SCL":     ("I2C_SCL", "gemeinsam: PCM1863 und DS3231"),
    "I2C_SDA":     ("I2C_SDA", ""),
    "RTC_SQW":     ("RTC_SQW", "1 Hz von der DS3231, latcht den Samplezaehler"),
    "SD_CLK":      ("SD_CLK", "SDMMC, 4 Bit"),
    "SD_CMD":      ("SD_CMD", ""),
    "SD_D0":       ("SD_D0", ""),
    "SD_D1":       ("SD_D1", ""),
    "SD_D2":       ("SD_D2", ""),
    "SD_D3":       ("SD_D3", ""),
    "BTN_REC":     ("BTN_REC", "Taster, gegen Masse"),
    "BTN_MARK":    ("BTN_MARK", "Taster, gegen Masse"),
    "LED_REC":     ("LED_REC", ""),
    "LED_ERR":     ("LED_ERR", ""),
    "LED_SYNC":    ("LED_SYNC", "Blitz fuer den Videomarker"),
    "BUZZ":        ("BUZZ", "Piezo ueber Q4"),
    "CAN_TX":      ("CAN_TX", "an den SN65HVD230"),
    "CAN_RX":      ("CAN_RX", ""),
    "PGOOD":       ("PGOOD", "Sammelmeldung der Versorgung"),
    "PWR_HOLD":    ("PWR_HOLD", "Selbsthaltung: haelt EN/UVLO des LM5164"),
    "IGN_SENSE":   ("IGN_SENSE", "Zuendung ueber Teiler, ADC1"),
    "VBAT_SENSE":  ("VBAT_SENSE", "Bordnetz ueber Teiler, ADC1"),
    "GPS_TX":      ("GPS_TX", "Stiftleiste J6, unbestueckt"),
    "GPS_RX":      ("GPS_RX", ""),
    "GPS_PPS":     ("GPS_PPS", ""),
    "UART0_TX":    ("UART0_TX", "Konsole"),
    "UART0_RX":    ("UART0_RX", ""),
}
# Pinfunktionen ohne IOxx im Namen
SONDER = {"RXD0": 44, "TXD0": 43}


def lesen(pfad: str) -> dict[str, tuple[int, int]]:
    """Netzname -> (GPIO, Modulpin) fuer alle Pins von U6.

    Die Netzzuordnung kommt aus pcbgen.load_netlist - dieselbe Funktion,
    die auch die Platine erzeugt. Ein eigener Parser fuer das Netzformat
    waere eine zweite Stelle, die bei einem KiCad-Wechsel bricht.
    """
    sys.path.insert(0, os.path.join(HIER, "tools"))
    from pcbgen import load_netlist                        # noqa: E402

    text = open(pfad, encoding="utf-8").read()
    funktion: dict[str, str] = {}
    for m in re.finditer(
            r'\(ref "U6"\)\s*\(pin "([^"]+)"\)\s*\(pinfunction "([^"]*)"\)',
            text):
        funktion[m.group(1)] = m.group(2)

    netz = load_netlist(pfad)
    aus: dict[str, tuple[int, int]] = {}
    for (ref, pin), name in netz.pad_net.items():
        if ref != "U6" or not pin.isdigit():
            continue
        f = funktion.get(pin, "")
        g = re.match(r"IO(\d+)_", f)
        if g:
            aus[name] = (int(g.group(1)), int(pin))
            continue
        for kennung, gpio in SONDER.items():
            if f.startswith(kennung):
                aus[name] = (gpio, int(pin))
    return aus


def schreiben(belegung: dict[str, tuple[int, int]], ziel: str) -> int:
    fehlend = [n for n in NETZE if n not in belegung]
    zeilen = [
        "/* Erzeugt von firmware/tools/pins_aus_netzliste.py - nicht von Hand",
        " * aendern. Quelle ist der Schaltplan ueber output/main.net.",
        " *",
        " * Modul: ESP32-S3-WROOM-1-N8R2, 8 MB Flash, 2 MB PSRAM (quad).",
        " * Beim N8R2 sind GPIO35..37 frei - bei den Octal-PSRAM-Varianten",
        " * waeren sie belegt und die GPS-Stiftleiste unbrauchbar.",
        " */",
        "",
        "#pragma once",
        "",
    ]
    breite = max(len(v[0]) for v in NETZE.values()) + 5
    for netz, (name, komm) in NETZE.items():
        if netz not in belegung:
            continue
        gpio, modulpin = belegung[netz]
        eintrag = f"#define PIN_{name}"
        zeilen.append(f"{eintrag:<{breite + 12}}{gpio:>3}"
                      f"   /* Modulpin {modulpin:>2}"
                      + (f", {komm}" if komm else "") + " */")
    zeilen.append("")
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    with open(ziel, "w", encoding="utf-8") as fh:
        fh.write("\n".join(zeilen))
    print(f"geschrieben: {ziel}  ({len(belegung)} Netze an U6, "
          f"{sum(1 for n in NETZE if n in belegung)} im Header)")
    if fehlend:
        print("  ! nicht in der Netzliste gefunden: " + " ".join(fehlend))
    return len(fehlend)


if __name__ == "__main__":
    netzliste = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        HIER, "output", "main.net")
    rc = schreiben(lesen(netzliste),
                   os.path.join(HIER, "firmware", "main", "pins.h"))
    sys.exit(1 if rc else 0)
