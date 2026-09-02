/* PCM1863: zweikanaliger Wandler, differentielle Eingaenge, Taktherr.
 * Anforderung F5 der Spezifikation.
 *
 * Registerkarte und Bitfelder sind nicht geraten, sondern aus dem
 * Linux-Treiber sound/soc/codecs/pcm186x.{c,h} uebernommen - der ist
 * gegen echte Bausteine gelaufen.
 */
#pragma once
#include <stdint.h>
#include "esp_err.h"

#define PCM1863_PGA_MIN_DB (-12.0f)
#define PCM1863_PGA_MAX_DB (32.0f)      /* Datenblatt kann 40, Spec sagt 32 */

esp_err_t pcm1863_start(uint32_t abtastrate, uint8_t bits_je_wort);
esp_err_t pcm1863_pga_setzen(float kanal_a_db, float kanal_b_db);
esp_err_t pcm1863_takt_pruefen(uint8_t *status_aus);
