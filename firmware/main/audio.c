#include "audio.h"
#include "pins.h"

#include <string.h>
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio";

/* 8 Puffer a 1024 Rahmen: rund 170 ms Vorlauf, bevor eine Schreibpause
 * der Karte zu einer Luecke wird. */
#define DMA_PUFFER   8
#define DMA_RAHMEN   1024

static i2s_chan_handle_t   KANAL;
static volatile uint64_t   RAHMEN;
static volatile int32_t    SPITZE[AUDIO_KANAELE];

esp_err_t audio_start(void)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                        I2S_ROLE_SLAVE);
    chan.dma_desc_num = DMA_PUFFER;
    chan.dma_frame_num = DMA_RAHMEN;
    chan.auto_clear = false;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, NULL, &KANAL), TAG, "Kanal");

    i2s_std_config_t cfg = {
        /* Die Taktvorgabe wird im Slave-Betrieb nicht benutzt, muss aber
         * gesetzt sein; BCK und LRCK kommen vom PCM1863. */
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   /* der Wandler hat seinen eigenen */
            .bclk = PIN_I2S_BCK,
            .ws   = PIN_I2S_LRCK,
            .dout = I2S_GPIO_UNUSED,
            .din  = PIN_I2S_DIN,
            .invert_flags = {false, false, false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(KANAL, &cfg), TAG, "Modus");
    RAHMEN = 0;
    ESP_RETURN_ON_ERROR(i2s_channel_enable(KANAL), TAG, "einschalten");
    ESP_LOGI(TAG, "I2S laeuft als Mitlaeufer, %u Hz, %u Bit",
             AUDIO_RATE, AUDIO_BITS);
    return ESP_OK;
}

esp_err_t audio_stop(void)
{
    if (!KANAL) {
        return ESP_OK;
    }
    i2s_channel_disable(KANAL);
    esp_err_t e = i2s_del_channel(KANAL);
    KANAL = NULL;
    return e;
}

size_t audio_lesen(void *ziel, size_t max, uint32_t warten_ms)
{
    size_t gelesen = 0;
    if (!KANAL) {
        return 0;
    }
    if (i2s_channel_read(KANAL, ziel, max, &gelesen,
                         pdMS_TO_TICKS(warten_ms)) != ESP_OK) {
        return 0;
    }
    RAHMEN += gelesen / AUDIO_BYTES_JE_RAHMEN;

    /* Spitzenwert je Kanal mitfuehren. Der PCM1863 liefert 24 Bit
     * linksbuendig im 32-Bit-Slot, die unteren acht Bit sind Null. */
    const int32_t *p = (const int32_t *)ziel;
    size_t n = gelesen / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) {
        int32_t v = p[i] < 0 ? -p[i] : p[i];
        int k = (int)(i & 1u);
        if (v > SPITZE[k]) {
            SPITZE[k] = v;
        }
    }
    return gelesen;
}

volatile uint64_t *audio_rahmenzaehler(void) { return &RAHMEN; }

float audio_spitze(int kanal)
{
    if (kanal < 0 || kanal >= (int)AUDIO_KANAELE) {
        return 0.0f;
    }
    int32_t v = SPITZE[kanal];
    SPITZE[kanal] = 0;
    return (float)v / 2147483648.0f;
}
