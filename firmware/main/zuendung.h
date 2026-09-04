/* Zuendungsautomat und Selbsthaltung (F7).
 *
 * Die Platine haengt mit VBAT12 an Klemme 30 (Dauerplus), ihre Freigabe
 * EN/UVLO aber an Klemme 15. Zuendung an heisst also: der Wandler laeuft
 * an, der ESP32 bootet. Zuendung aus wuerde den Strom mitten im Schreiben
 * abstellen - deshalb haelt PWR_HOLD die Freigabe ueber eine Diode fest,
 * bis die Datei geschlossen ist.
 *
 * PWR_HOLD muss als Allererstes in app_main gesetzt werden. Bricht die
 * Zuendspannung beim Anlassen ein, faellt die Platine sonst genau dann
 * wieder aus, wenn die Aufnahme laufen soll.
 *
 * Gemessen wird ueber die beiden Teiler auf dem Versorgungsblatt:
 *
 *   Klemme 15  R9 100k / R10 47k, Zenerklemme D5 -> ab rund 10 V gekappt
 *   Bordnetz   R11 470k / R12 68k, ungekappt bis ueber 20 V
 *
 * Der Bordnetzwert ist der eigentliche Motorlauf-Melder: steht der Motor,
 * liegt die Ruhespannung des Akkus an, laeuft er, haelt die Lichtmaschine
 * gut ein Volt mehr.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    ZUENDUNG_AUS,       /* Klemme 15 spannungslos */
    ZUENDUNG_AN,        /* Zuendung an, Motor steht */
    ZUENDUNG_MOTOR,     /* Lichtmaschine laedt - der Motor laeuft */
} zuendung_t;

/* Freigabe halten. Ohne Messtechnik, ohne Speicher, als erste Zeile in
 * app_main aufzurufen. */
void zuendung_selbsthaltung(void);

/* Messung und Automat starten. `melden` wird bei jedem Zustandswechsel
 * aus der eigenen Aufgabe heraus gerufen, nicht aus einer Unterbrechung. */
esp_err_t zuendung_start(void (*melden)(zuendung_t, void *), void *arg);

zuendung_t zuendung_zustand(void);
float zuendung_bordspannung(void);   /* V, Klemme 30 */
float zuendung_klemme15(void);       /* V, oberhalb rund 10 V gekappt */

/* PGOOD des LM5164, nur zur Anzeige. Waehrend der Wandler regelt, ist der
 * Ausgang hochohmig und der Pullup im Modul zieht ihn hoch; faellt die
 * Ausgangsspannung unter 90 Prozent, zieht der Wandler nach Masse. Wie
 * sich der Pin im Ruhezustand verhaelt, sagt das Datenblatt nicht -
 * darauf darf keine Entscheidung aufbauen. */
bool zuendung_wandler_laeuft(void);

/* Selbsthaltung loesen. Am Fahrzeug geht die Platine damit aus und der
 * Aufruf kehrt nie zurueck; haengt sie zusaetzlich an USB, wartet er drei
 * Sekunden und kommt zurueck. Die Datei muss vorher geschlossen sein. */
void zuendung_abschalten(void);
