/* Taster, Leuchtdioden, Piezo. Enthaelt den Videomarker (F8):
 * Blitz und Piep gleichzeitig, Samplenummer ins Protokoll.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum { LED_REC, LED_ERR, LED_SYNC } led_t;

esp_err_t bedienung_start(void);
void      led(led_t welche, bool an);
bool      taste_rec_geholt(void);    /* true genau einmal je Druck */
bool      taste_mark_geholt(void);
void      marker_ausloesen(void);    /* Blitz + Piep, blockiert 120 ms */
