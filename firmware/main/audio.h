/* Aufnahmepfad: I2S als Mitlaeufer, DMA, Ringpuffer im PSRAM (F1).
 *
 * Der Wandler ist Taktherr. Der ESP32 empfaengt nur - deshalb Slave.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_RATE      48000u
#define AUDIO_BITS      32u        /* Slotbreite; der PCM1863 liefert 24 Bit linksbuendig */
#define AUDIO_KANAELE   2u
#define AUDIO_BYTES_JE_RAHMEN (AUDIO_KANAELE * (AUDIO_BITS / 8))

esp_err_t audio_start(void);
esp_err_t audio_stop(void);

/* Blockierend bis zu `max` Bytes holen. Rueckgabe: gelesene Bytes. */
size_t audio_lesen(void *ziel, size_t max, uint32_t warten_ms);

/* Fortlaufender Rahmenzaehler seit audio_start(). Die Zeitbasis latcht
 * ihn bei jeder Sekundenflanke der Uhr. */
volatile uint64_t *audio_rahmenzaehler(void);

/* Groesster Betrag seit dem letzten Aufruf, 0..1. Fuer die
 * Uebersteuerungsanzeige (F6). */
float audio_spitze(int kanal);
