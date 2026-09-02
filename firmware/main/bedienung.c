#include "bedienung.h"
#include "pins.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bedienung";

#define ENTPRELLEN_US 25000
#define PIEP_HZ       2700          /* nahe der Resonanz des CPT-9019A */

static struct {
    int  pin;
    bool letzter;
    int64_t wechsel;
    bool geholt;
} T[2] = {{.pin = PIN_BTN_REC}, {.pin = PIN_BTN_MARK}};

static void taste_pruefen(int i)
{
    bool jetzt = gpio_get_level(T[i].pin) == 0;   /* gegen Masse */
    int64_t t = esp_timer_get_time();
    if (jetzt != T[i].letzter) {
        if (t - T[i].wechsel > ENTPRELLEN_US) {
            T[i].letzter = jetzt;
            T[i].wechsel = t;
            if (jetzt) {
                T[i].geholt = true;
            }
        }
    } else {
        T[i].wechsel = t;
    }
}

esp_err_t bedienung_start(void)
{
    gpio_config_t aus = {
        .pin_bit_mask = (1ULL << PIN_LED_REC) | (1ULL << PIN_LED_ERR) |
                        (1ULL << PIN_LED_SYNC),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&aus), TAG, "Leuchtdioden");

    gpio_config_t ein = {
        .pin_bit_mask = (1ULL << PIN_BTN_REC) | (1ULL << PIN_BTN_MARK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&ein), TAG, "Taster");

    ledc_timer_config_t lt = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PIEP_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&lt), TAG, "Piezotakt");
    ledc_channel_config_t lc = {
        .gpio_num = PIN_BUZZ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    return ledc_channel_config(&lc);
}

void led(led_t welche, bool an)
{
    int pin = welche == LED_REC ? PIN_LED_REC
            : welche == LED_ERR ? PIN_LED_ERR : PIN_LED_SYNC;
    gpio_set_level(pin, an ? 1 : 0);
}

static bool holen(int i)
{
    taste_pruefen(i);
    bool g = T[i].geholt;
    T[i].geholt = false;
    return g;
}

bool taste_rec_geholt(void)  { return holen(0); }
bool taste_mark_geholt(void) { return holen(1); }

void marker_ausloesen(void)
{
    /* Blitz und Piep muessen zusammen kommen: die Kamera sieht das eine
     * oder hoert das andere, und im Schnitt wird darauf ausgerichtet. */
    led(LED_SYNC, true);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(120));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    led(LED_SYNC, false);
}
