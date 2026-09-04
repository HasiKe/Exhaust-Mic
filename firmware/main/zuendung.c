#include "zuendung.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

static const char *TAG = "zuendung";

/* Teilerverhaeltnisse aus dem Schaltplan, Blatt Versorgung. Wandern die
 * Widerstaende, wandern diese Zahlen mit - sie stehen absichtlich hier
 * und nicht in pins.h, denn pins.h kennt nur GPIO-Nummern. */
#define TEILER_IGN    (147.0f / 47.0f)      /* R9 100k / R10 47k  */
#define TEILER_VBAT   (538.0f / 68.0f)      /* R11 470k / R12 68k */

/* Schwellen fuer Klemme 15.
 *
 * Die Abschaltschwelle liegt bewusst tief. Beim Anlassen bricht das
 * Bordnetz ein, und die Zuendleitung geht mit; wer hier 9 V ansetzt,
 * schaltet das Geraet genau beim Motorstart ab. Der Schluesseldreh
 * dagegen faellt hart auf null, den erkennt auch 5 V sicher.
 *
 * Die Hardware selbst gibt bei 8,3 V frei und bei 7,8 V wieder ab
 * (1,5 V bzw. 1,4 V Freigabeschwelle am LM5164 ueber R2 680k / R3 150k).
 * Solange PWR_HOLD haelt, entscheidet aber allein die Firmware. */
#define IGN_AN_V       7.0f
#define IGN_AUS_V      5.0f
#define IGN_AN_MS       300
#define IGN_AUS_MS     2000

/* Schwellen fuer den Motorlauf. Ruhespannung eines vollen Akkus sind
 * 12,6 bis 12,8 V, direkt nach der Fahrt haelt die Oberflaechenladung
 * ein paar Minuten bis 13,0 V. Die Lichtmaschine regelt auf 14,0 bis
 * 14,4 V. 13,2 V trennt beides, mit reichlich Abstand nach unten.
 *
 * Im Leerlauf mit Luefter und Licht kann die Spannung kurz einbrechen -
 * daher die lange Haltezeit beim Abfallen. */
#define MOTOR_AN_V     13.2f
#define MOTOR_AUS_V    12.9f
#define MOTOR_AN_MS     2000
#define MOTOR_AUS_MS   10000

#define TAKT_MS         100
#define MITTELUNG         16     /* der ADC des ESP32-S3 rauscht sichtbar */

static adc_oneshot_unit_handle_t ADC1;
static adc_cali_handle_t KALIBRIERUNG;
static volatile zuendung_t ZUSTAND = ZUENDUNG_AUS;
static volatile float U_IGN, U_VBAT;

/* Auf dem ESP32-S3 liegen die Kanaele 0..9 von ADC1 auf GPIO1..GPIO10.
 * Damit folgt der Kanal aus der erzeugten Pinbelegung, statt ihn ein
 * zweites Mal von Hand einzutragen. */
#define KANAL(gpio) ((adc_channel_t)((gpio) - 1))

static float messen(adc_channel_t kanal, float teiler)
{
    int summe = 0;
    for (int i = 0; i < MITTELUNG; i++) {
        int roh = 0;
        if (adc_oneshot_read(ADC1, kanal, &roh) != ESP_OK) {
            return -1.0f;
        }
        summe += roh;
    }
    int mv = 0;
    if (adc_cali_raw_to_voltage(KALIBRIERUNG, summe / MITTELUNG, &mv) != ESP_OK) {
        return -1.0f;
    }
    return mv / 1000.0f * teiler;
}

/* Schwellwertschalter mit Haltezeit: `zeit` zaehlt die Takte, die das
 * Signal schon auf der anderen Seite steht. Erst wenn es lange genug
 * dort bleibt, kippt der Ausgang. */
static bool halten(bool zustand, bool ueber, bool unter, int *zeit,
                   int an_ms, int aus_ms)
{
    bool ziel = zustand ? !unter : ueber;
    if (ziel == zustand) {
        *zeit = 0;
        return zustand;
    }
    *zeit += TAKT_MS;
    if (*zeit >= (ziel ? an_ms : aus_ms)) {
        *zeit = 0;
        return ziel;
    }
    return zustand;
}

struct anmeldung {
    void (*ruf)(zuendung_t, void *);
    void *nutzdaten;
};

static void automat(void *arg)
{
    const struct anmeldung *an = arg;

    bool zuendung = false, motor = false, war_schon_an = false;
    int t_zuendung = 0, t_motor = 0;
    zuendung_t letzter = ZUENDUNG_AUS;

    while (true) {
        /* Ein misslungener Wandlerlauf darf nicht als "Zuendung aus"
         * durchgehen - dann bliebe der letzte gute Wert stehen. */
        float ign = messen(KANAL(PIN_IGN_SENSE), TEILER_IGN);
        float bat = messen(KANAL(PIN_VBAT_SENSE), TEILER_VBAT);
        ign = ign >= 0.0f ? (U_IGN = ign) : U_IGN;
        bat = bat >= 0.0f ? (U_VBAT = bat) : U_VBAT;

        zuendung = halten(zuendung, ign > IGN_AN_V, ign < IGN_AUS_V,
                          &t_zuendung, IGN_AN_MS, IGN_AUS_MS);
        motor = halten(motor, bat > MOTOR_AN_V, bat < MOTOR_AUS_V,
                       &t_motor, MOTOR_AN_MS, MOTOR_AUS_MS);

        zuendung_t jetzt = !zuendung ? ZUENDUNG_AUS
                         : motor     ? ZUENDUNG_MOTOR
                                     : ZUENDUNG_AN;
        /* Am Schreibtisch haengt die Platine an USB, Klemme 15 liegt auf
         * null - ohne diese Sperre meldete der Automat dort sofort
         * "Zuendung aus" und die Aufnahme liefe nie an. Erst wer die
         * Zuendung einmal gesehen hat, darf sie auch vermissen. */
        if (jetzt != ZUENDUNG_AUS) {
            war_schon_an = true;
        }
        if (jetzt != letzter && (war_schon_an || jetzt != ZUENDUNG_AUS)) {
            ESP_LOGI(TAG, "%s (Klemme 15: %.1f V, Bordnetz: %.1f V)",
                     jetzt == ZUENDUNG_AUS ? "Zuendung aus"
                     : jetzt == ZUENDUNG_MOTOR ? "Motor laeuft"
                                               : "Zuendung an",
                     (double)U_IGN, (double)U_VBAT);
            letzter = jetzt;
            ZUSTAND = jetzt;
            if (an->ruf) {
                an->ruf(jetzt, an->nutzdaten);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TAKT_MS));
    }
}

void zuendung_selbsthaltung(void)
{
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << PIN_PWR_HOLD,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&c);
    gpio_set_level(PIN_PWR_HOLD, 1);
}

esp_err_t zuendung_start(void (*melden)(zuendung_t, void *), void *arg)
{
    static struct anmeldung an;
    an.ruf = melden;
    an.nutzdaten = arg;

    /* PGOOD hat keinen aeusseren Pullup - der Ausgang des LM5164 ist
     * offener Kollektor. Der Pullup im Modul reicht fuer die Abfrage. */
    gpio_config_t p = {
        .pin_bit_mask = 1ULL << PIN_PGOOD,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&p));

    adc_oneshot_unit_init_cfg_t einheit = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&einheit, &ADC1));

    /* 12 dB Daempfung: der Eingang reicht damit bis rund 3,1 V. Das
     * Bordnetz kommt ueber den Teiler bei 20 V erst auf 2,5 V. */
    adc_oneshot_chan_cfg_t kanal = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ADC1, KANAL(PIN_IGN_SENSE),
                                               &kanal));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ADC1, KANAL(PIN_VBAT_SENSE),
                                               &kanal));

    adc_cali_curve_fitting_config_t kal = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t rc = adc_cali_create_scheme_curve_fitting(&kal, &KALIBRIERUNG);
    if (rc != ESP_OK) {
        /* Ohne die Werkskalibrierung im eFuse waeren die Spannungen um
         * bis zu zehn Prozent daneben - das faellt genau zwischen
         * Ruhespannung und Ladespannung. Dann lieber nicht raten. */
        ESP_LOGE(TAG, "ADC-Kalibrierung fehlt (%s)", esp_err_to_name(rc));
        return rc;
    }

    if (xTaskCreate(automat, "zuendung", 3072, &an, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Automat laeuft, Wandler %s",
             zuendung_wandler_laeuft() ? "regelt" : "steht (USB)");
    return ESP_OK;
}

zuendung_t zuendung_zustand(void)   { return ZUSTAND; }
float zuendung_bordspannung(void)   { return U_VBAT; }
float zuendung_klemme15(void)       { return U_IGN; }

bool zuendung_wandler_laeuft(void)
{
    return gpio_get_level(PIN_PGOOD) != 0;
}

void zuendung_abschalten(void)
{
    ESP_LOGW(TAG, "Selbsthaltung faellt, Bordnetz %.1f V", (double)U_VBAT);
    gpio_set_level(PIN_PWR_HOLD, 0);
    /* Am Fahrzeug bricht die Versorgung hier binnen Millisekunden weg -
     * die Wartezeit laeuft dann nie ab. Wer sie ueberlebt, haengt am
     * USB-Anschluss und bleibt einfach an.
     *
     * Bewusst nach der Uhr und nicht nach PGOOD: das Datenblatt sagt
     * nicht, ob der Ausgang im Ruhezustand des Wandlers noch nach Masse
     * zieht oder hochohmig wird. Der Pullup im Modul wuerde ihn dann als
     * "regelt" melden. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "noch da - Versorgung kommt aus USB, Geraet bleibt an");
}
