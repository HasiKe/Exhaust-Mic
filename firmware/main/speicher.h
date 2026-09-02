/* Karte und Datei. Anforderungen F2 und F3.
 *
 * Geschrieben wird in eine vorallokierte Datei; der WAV-Kopf bekommt die
 * richtigen Laengen erst beim Schliessen. Faellt unterwegs der Strom aus,
 * kostet das hoechstens den letzten Block - der Rest ist ueber den
 * vorbelegten Kopf weiterhin lesbar.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

esp_err_t speicher_start(void);
bool      speicher_bereit(void);

/* Neue Aufnahme. `start` ist die Wanduhrzeit, `erste_probe` die
 * Samplenummer seit Mitternacht - beides landet im BEXT-Block, damit
 * Resolve und Premiere die Datei von selbst einsortieren. */
esp_err_t speicher_datei_oeffnen(const struct tm *start, uint64_t erste_probe);
esp_err_t speicher_schreiben(const void *daten, size_t bytes);
esp_err_t speicher_datei_schliessen(void);
bool      speicher_laeuft(void);
uint64_t  speicher_bytes(void);

/* Zeile in die Begleitdatei (F4, F8, F10). */
esp_err_t speicher_notiz(const char *format, ...);
