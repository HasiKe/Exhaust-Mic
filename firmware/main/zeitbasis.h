/* Zeitbasis: DS3231 als Wanduhr, ihr 1-Hz-Ausgang als Anker fuer die
 * Samplenummer (Anforderung F4 der Spezifikation).
 *
 * Der Oszillator des Wandlers und die Uhr laufen unabhaengig. Ohne Anker
 * driftet die Zuordnung Zeit <-> Sample ueber eine lange Aufnahme um
 * mehrere Sekunden. Mit dem Anker haelt sie ueber die volle Dauer.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

typedef struct {
    time_t   utc;         /* Sekunde laut DS3231 */
    uint64_t sample;      /* Samplezaehler bei der Sekundenflanke */
} zeit_anker_t;

esp_err_t zeitbasis_start(void);
esp_err_t zeitbasis_lesen(struct tm *aus);
esp_err_t zeitbasis_stellen(const struct tm *neu);

/* Letzter Anker. Gibt false, solange noch keine Flanke kam. */
bool zeitbasis_anker(zeit_anker_t *aus);

/* Vom Audiopfad gerufen, damit die Unterbrechung weiss, wo sie steht. */
void zeitbasis_samplezaehler_setzen(volatile uint64_t *zaehler);
