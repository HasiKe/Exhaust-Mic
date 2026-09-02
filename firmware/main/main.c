/* exhaust-mic: zweikanaliger Rekorder mit GoPro-Kopplung.
 *
 * Ablauf: Wandler und Karte hochfahren, dann auf drei Ausloeser warten -
 * die Taste am Geraet, die Kamera ueber Bluetooth LE, spaeter die
 * Zuendung. Jeder startet dieselbe Aufnahme.
 *
 * Umgesetzt: F1 (I2S per DMA), F2/F3 (WAV mit BEXT), F4 (Sekundenanker),
 * F5 (Wandler ueber I2C), F6 (Uebersteuerung), F8 (Marker) sowie die
 * GoPro-Kopplung. Offen: F7 Zuendungsautomat, F9 WLAN und NTP, F10 CAN,
 * F11 Download - siehe firmware/README.md.
 */

#include <inttypes.h>
#include <string.h>

#include "audio.h"
#include "bedienung.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gopro.h"
#include "i2cbus.h"
#include "pcm1863.h"
#include "speicher.h"
#include "zeitbasis.h"

static const char *TAG = "main";

/* 16 kB je Schreibvorgang: gross genug, dass die Karte am Stueck
 * arbeitet, klein genug, dass der DMA-Vorlauf nicht ausgeht. */
#define BLOCK 16384

static QueueHandle_t BEFEHLE;      /* true = starten, false = stoppen */

static void gopro_meldet(bool laeuft, void *arg)
{
    BaseType_t geweckt = pdFALSE;
    xQueueSendFromISR(BEFEHLE, &laeuft, &geweckt);
    portYIELD_FROM_ISR(geweckt);
}

static void aufnahme_starten(void)
{
    if (speicher_laeuft()) {
        return;
    }
    struct tm jetzt;
    if (zeitbasis_lesen(&jetzt) != ESP_OK) {
        ESP_LOGE(TAG, "Uhr antwortet nicht, keine Aufnahme");
        led(LED_ERR, true);
        return;
    }
    /* TimeReference im BEXT zaehlt Samples seit Mitternacht. */
    uint64_t seit_mitternacht =
        ((uint64_t)jetzt.tm_hour * 3600 + jetzt.tm_min * 60 + jetzt.tm_sec) *
        AUDIO_RATE;
    if (speicher_datei_oeffnen(&jetzt, seit_mitternacht) != ESP_OK) {
        led(LED_ERR, true);
        return;
    }
    led(LED_REC, true);
    marker_ausloesen();
    speicher_notiz("%llu,marker,start", (unsigned long long)*audio_rahmenzaehler());
}

static void aufnahme_stoppen(void)
{
    if (!speicher_laeuft()) {
        return;
    }
    speicher_notiz("%llu,marker,stop", (unsigned long long)*audio_rahmenzaehler());
    speicher_datei_schliessen();
    led(LED_REC, false);
}

/* Schreibt, was der DMA liefert. Eigene Aufgabe, damit die Karte den
 * Rest des Systems nicht anhaelt. */
static void schreiber(void *arg)
{
    static uint8_t puffer[BLOCK];
    while (true) {
        size_t n = audio_lesen(puffer, sizeof puffer, 200);
        if (n && speicher_laeuft()) {
            if (speicher_schreiben(puffer, n) != ESP_OK) {
                led(LED_ERR, true);
                aufnahme_stoppen();
            }
        }
    }
}

/* Sekundenanker und Uebersteuerung. */
static void aufsicht(void *arg)
{
    zeit_anker_t letzter = {0}, jetzt;
    while (true) {
        if (speicher_laeuft() && zeitbasis_anker(&jetzt) &&
            jetzt.sample != letzter.sample) {
            letzter = jetzt;
            speicher_notiz("%llu,uhr,%lld", (unsigned long long)jetzt.sample,
                           (long long)jetzt.utc);
        }
        float a = audio_spitze(0), b = audio_spitze(1);
        /* -0,5 dBFS. Darueber ist der Wandler praktisch am Anschlag. */
        led(LED_ERR, a > 0.944f || b > 0.944f);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    BEFEHLE = xQueueCreate(4, sizeof(bool));

    ESP_ERROR_CHECK(bedienung_start());
    ESP_ERROR_CHECK(i2cbus_start());
    ESP_ERROR_CHECK(zeitbasis_start());
    ESP_ERROR_CHECK(pcm1863_start(AUDIO_RATE, 24));
    ESP_ERROR_CHECK(audio_start());
    zeitbasis_samplezaehler_setzen(audio_rahmenzaehler());
    pcm1863_takt_pruefen(NULL);

    if (speicher_start() != ESP_OK) {
        led(LED_ERR, true);
    }
    ESP_ERROR_CHECK(gopro_start(gopro_meldet, NULL));

    xTaskCreate(schreiber, "schreiber", 4096, NULL, 6, NULL);
    xTaskCreate(aufsicht, "aufsicht", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "bereit");

    while (true) {
        bool befehl;
        if (xQueueReceive(BEFEHLE, &befehl, pdMS_TO_TICKS(20)) == pdTRUE) {
            ESP_LOGI(TAG, "Kamera: %s", befehl ? "start" : "stop");
            befehl ? aufnahme_starten() : aufnahme_stoppen();
        }
        if (taste_rec_geholt()) {
            if (speicher_laeuft()) {
                aufnahme_stoppen();
                gopro_ausloesen(false);     /* Kamera mitnehmen */
            } else {
                aufnahme_starten();
                gopro_ausloesen(true);
            }
        }
        if (taste_mark_geholt() && speicher_laeuft()) {
            uint64_t s = *audio_rahmenzaehler();
            marker_ausloesen();
            speicher_notiz("%llu,marker,taste", (unsigned long long)s);
        }
    }
}
