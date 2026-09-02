#include "zeitbasis.h"
#include "i2cbus.h"
#include "pins.h"

#include <string.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "zeit";
#define DS3231 0x68
#define REG_ZEIT 0x00
#define REG_STEUER 0x0E

static i2c_master_dev_handle_t UHR;
static volatile uint64_t      *SAMPLES;
static volatile zeit_anker_t   ANKER;
static volatile bool           HAT_ANKER;

static uint8_t bcd_zu_bin(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0f)); }
static uint8_t bin_zu_bcd(uint8_t b) { return (uint8_t)(((b / 10) << 4) | (b % 10)); }

/* Nur den Samplezaehler latchen. Alles Weitere gehoert nicht in eine
 * Unterbrechung - die Sekundenflanke ist der einzige Zeitpunkt, an dem
 * Zaehlerstand und Wanduhr garantiert zusammenpassen. */
static void IRAM_ATTR sqw_isr(void *arg)
{
    ANKER.sample = SAMPLES ? *SAMPLES : 0;
    ANKER.utc = 0;                 /* die Sekunde holt die Aufgabe nach */
    HAT_ANKER = true;
}

esp_err_t zeitbasis_start(void)
{
    ESP_RETURN_ON_ERROR(i2cbus_geraet(DS3231, &UHR), TAG, "DS3231 fehlt");

    /* Steuerregister: BBSQW ein, INTCN aus, RS1/RS2 = 0 -> 1 Hz am SQW.
     * Der Ausgang ist offener Kollektor, R42 zieht hoch. */
    const uint8_t steuern[2] = {REG_STEUER, 0x40};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(UHR, steuern, sizeof steuern, 100),
                        TAG, "Steuerregister");

    gpio_config_t g = {
        .pin_bit_mask = 1ULL << PIN_RTC_SQW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&g), TAG, "SQW-Pin");
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    return gpio_isr_handler_add(PIN_RTC_SQW, sqw_isr, NULL);
}

void zeitbasis_samplezaehler_setzen(volatile uint64_t *zaehler)
{
    SAMPLES = zaehler;
}

esp_err_t zeitbasis_lesen(struct tm *aus)
{
    uint8_t reg = REG_ZEIT, d[7];
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(UHR, &reg, 1, d, sizeof d, 100),
        TAG, "Zeit lesen");
    memset(aus, 0, sizeof *aus);
    aus->tm_sec  = bcd_zu_bin(d[0] & 0x7f);
    aus->tm_min  = bcd_zu_bin(d[1] & 0x7f);
    aus->tm_hour = bcd_zu_bin(d[2] & 0x3f);      /* 24-Stunden-Modus */
    aus->tm_mday = bcd_zu_bin(d[4] & 0x3f);
    aus->tm_mon  = bcd_zu_bin(d[5] & 0x1f) - 1;
    aus->tm_year = bcd_zu_bin(d[6]) + 100;       /* seit 1900 */
    return ESP_OK;
}

esp_err_t zeitbasis_stellen(const struct tm *neu)
{
    const uint8_t d[8] = {
        REG_ZEIT,
        bin_zu_bcd((uint8_t)neu->tm_sec),
        bin_zu_bcd((uint8_t)neu->tm_min),
        bin_zu_bcd((uint8_t)neu->tm_hour),
        (uint8_t)(neu->tm_wday + 1),
        bin_zu_bcd((uint8_t)neu->tm_mday),
        bin_zu_bcd((uint8_t)(neu->tm_mon + 1)),
        bin_zu_bcd((uint8_t)(neu->tm_year % 100)),
    };
    return i2c_master_transmit(UHR, d, sizeof d, 100);
}

bool zeitbasis_anker(zeit_anker_t *aus)
{
    if (!HAT_ANKER) {
        return false;
    }
    zeit_anker_t a = {.sample = ANKER.sample};
    struct tm t;
    if (zeitbasis_lesen(&t) != ESP_OK) {
        return false;
    }
    a.utc = mktime(&t);
    *aus = a;
    return true;
}
