/* Anbindung an eine GoPro ueber Bluetooth LE (Open GoPro).
 *
 * Zweck: die Aufnahme automatisch mitlaufen lassen. Sobald die Kamera zu
 * kodieren beginnt, startet der Rekorder; hoert sie auf, stoppt er.
 * Umgekehrt kann der Rekorder die Kamera ausloesen.
 *
 * Was hier NICHT geht: die Kamera als Tonquelle beliefern. GoPro nimmt
 * drahtlosen Ton nur ueber das Bluetooth-Hands-Free-Profil an, und das ist
 * Bluetooth Classic. Der ESP32-S3 hat ausschliesslich Bluetooth LE - weder
 * Bluedroid noch NimBLE koennen auf diesem Baustein Classic. Selbst wenn:
 * HFP ist einkanalig mit 8 oder 16 kHz Abtastrate. Diese Platine existiert
 * fuer zwei Kanaele mit 24 Bit. Siehe README, Abschnitt "GoPro".
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    GOPRO_AUS,            /* Funk aus oder nicht gestartet */
    GOPRO_SUCHT,          /* scannt nach einer Kamera */
    GOPRO_VERBINDET,
    GOPRO_BEREIT,         /* verbunden, Statusmeldungen abonniert */
} gopro_zustand_t;

/* Wird gerufen, wenn die Kamera zu kodieren beginnt oder aufhoert.
 * Laeuft im NimBLE-Host-Task - nichts Langes darin tun. */
typedef void (*gopro_aufnahme_cb_t)(bool laeuft, void *benutzer);

/* Funk starten und im Hintergrund nach einer Kamera suchen. Die Kamera
 * muss beim ersten Mal im Kopplungsmodus sein (Einstellungen ->
 * Verbindungen -> Geraet verbinden -> GoPro Quik App). */
esp_err_t gopro_start(gopro_aufnahme_cb_t cb, void *benutzer);

/* Kamera ausloesen. true startet, false stoppt. */
esp_err_t gopro_ausloesen(bool starten);

gopro_zustand_t gopro_zustand(void);
bool gopro_nimmt_auf(void);
