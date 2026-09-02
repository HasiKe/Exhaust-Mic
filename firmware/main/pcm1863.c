#include "pcm1863.h"
#include "i2cbus.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "pcm1863";

/* MS/AD liegt auf Masse -> 0x4A. */
#define ADRESSE 0x4A

/* Seite 0. Alle Register unten liegen dort. */
#define R_PAGE            0
#define R_PGA_CH1_L       1
#define R_PGA_CH1_R       2
#define R_ADC1_SEL_L      6
#define R_ADC1_SEL_R      7
#define R_PCM_CFG        11
#define R_CLK_CTRL       32
#define R_BCK_DIV        38
#define R_LRK_DIV        39
#define R_POWER_CTRL    112
#define R_CLK_STATUS    117

/* PCM_CFG */
#define CFG_FMT_I2S       0x00
#define CFG_TX_WLEN_32    (0x00 << 2)
#define CFG_TX_WLEN_24    (0x01 << 2)

/* ADCx_INPUT_SEL: Index 16 der Eingangsliste ist das differentielle Paar.
 * Fuer ADC1L ist das {VIN1P, VIN1M}, fuer ADC1R {VIN2P, VIN2M} - genau so
 * ist die Platine verdrahtet: Mikrofon A auf VIN1, Mikrofon B auf VIN2. */
#define SEL_DIFF_PAAR     0x10

/* CLK_CTRL */
#define CLK_MST_MODE      (1 << 4)
#define CLK_CLKDET_EN     (1 << 0)

/* POWER_CTRL */
#define PWR_STBY          (1 << 0)

/* Der Oszillator Y1 auf der Platine. 24,576 MHz = 512 x 48 kHz. */
#define SCKI_HZ 24576000u

static i2c_master_dev_handle_t DEV;

static esp_err_t reg_schreiben(uint8_t reg, uint8_t wert)
{
    const uint8_t d[2] = {reg, wert};
    return i2c_master_transmit(DEV, d, sizeof d, 100);
}

static esp_err_t reg_lesen(uint8_t reg, uint8_t *wert)
{
    return i2c_master_transmit_receive(DEV, &reg, 1, wert, 1, 100);
}

static uint8_t pga_byte(float db)
{
    if (db < PCM1863_PGA_MIN_DB) db = PCM1863_PGA_MIN_DB;
    if (db > PCM1863_PGA_MAX_DB) db = PCM1863_PGA_MAX_DB;
    /* 0,5 dB je Schritt, vorzeichenbehaftet. */
    int schritte = (int)(db * 2.0f + (db >= 0 ? 0.5f : -0.5f));
    return (uint8_t)(int8_t)schritte;
}

esp_err_t pcm1863_start(uint32_t abtastrate, uint8_t bits_je_wort)
{
    ESP_RETURN_ON_ERROR(i2cbus_geraet(ADRESSE, &DEV), TAG, "kein PCM1863");
    ESP_RETURN_ON_ERROR(reg_schreiben(R_PAGE, 0), TAG, "Seite 0");

    /* Standby, solange umkonfiguriert wird. */
    ESP_RETURN_ON_ERROR(reg_schreiben(R_POWER_CTRL, PWR_STBY), TAG, "Standby");

    /* Beide Kanaele differentiell. Ohne das laeuft der Wandler
     * unsymmetrisch und die Gleichtaktunterdrueckung des Kabels ist weg. */
    ESP_RETURN_ON_ERROR(reg_schreiben(R_ADC1_SEL_L, SEL_DIFF_PAAR),
                        TAG, "Eingang A");
    ESP_RETURN_ON_ERROR(reg_schreiben(R_ADC1_SEL_R, SEL_DIFF_PAAR),
                        TAG, "Eingang B");

    uint8_t wlen = (bits_je_wort == 24) ? CFG_TX_WLEN_24 : CFG_TX_WLEN_32;
    ESP_RETURN_ON_ERROR(reg_schreiben(R_PCM_CFG, (uint8_t)(wlen | CFG_FMT_I2S)),
                        TAG, "Format");

    /* Taktherr. Der Wandler haengt am eigenen Oszillator und erzeugt BCK
     * und LRCK selbst; der ESP32 ist am I2S nur Mitlaeufer. Umgekehrt
     * muesste der ESP32 den Takt aus seiner PLL ableiten, und deren
     * Jitter landet direkt im Abtastzeitpunkt. */
    const uint32_t bck_je_rahmen = 2u * bits_je_wort;
    const uint32_t bck_teiler = SCKI_HZ / (bck_je_rahmen * abtastrate);
    if (bck_teiler == 0 || bck_teiler > 256 ||
        SCKI_HZ != bck_teiler * bck_je_rahmen * abtastrate) {
        ESP_LOGE(TAG, "%" PRIu32 " Hz mit %u Bit passt nicht zu %u Hz SCKI",
                 abtastrate, bits_je_wort, SCKI_HZ);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(reg_schreiben(R_BCK_DIV, (uint8_t)(bck_teiler - 1)),
                        TAG, "BCK-Teiler");
    ESP_RETURN_ON_ERROR(reg_schreiben(R_LRK_DIV, (uint8_t)(bck_je_rahmen - 1)),
                        TAG, "LRCK-Teiler");
    ESP_RETURN_ON_ERROR(reg_schreiben(R_CLK_CTRL, CLK_MST_MODE | CLK_CLKDET_EN),
                        TAG, "Taktherr");

    ESP_RETURN_ON_ERROR(pcm1863_pga_setzen(0.0f, 0.0f), TAG, "PGA");
    ESP_RETURN_ON_ERROR(reg_schreiben(R_POWER_CTRL, 0), TAG, "Betrieb");

    ESP_LOGI(TAG, "%" PRIu32 " Hz, %u Bit, BCK-Teiler %" PRIu32
             ", %" PRIu32 " BCK je Rahmen",
             abtastrate, bits_je_wort, bck_teiler, bck_je_rahmen);
    return ESP_OK;
}

esp_err_t pcm1863_pga_setzen(float kanal_a_db, float kanal_b_db)
{
    ESP_RETURN_ON_ERROR(reg_schreiben(R_PGA_CH1_L, pga_byte(kanal_a_db)),
                        TAG, "PGA A");
    return reg_schreiben(R_PGA_CH1_R, pga_byte(kanal_b_db));
}

esp_err_t pcm1863_takt_pruefen(uint8_t *status_aus)
{
    uint8_t s = 0;
    esp_err_t e = reg_lesen(R_CLK_STATUS, &s);
    if (status_aus) {
        *status_aus = s;
    }
    /* Bit 6/5/4: LRCK/BCK/SCK stehen still. Bit 2/1: Taktfehler. */
    if (e == ESP_OK && (s & 0x76)) {
        ESP_LOGW(TAG, "Taktstatus 0x%02x - Oszillator oder Verdrahtung", s);
    }
    return e;
}
