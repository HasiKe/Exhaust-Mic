#include "speicher.h"
#include "audio.h"
#include "pins.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "speicher";
#define WURZEL "/sd"

static sdmmc_card_t *KARTE;
static FILE *WAV, *CSV;
static uint64_t GESCHRIEBEN;
/* Beim Schliessen muss derselbe BEXT-Block noch einmal geschrieben
 * werden - nur mit den jetzt bekannten Laengen. Startzeit und erste
 * Samplenummer deshalb merken; ein leeres struct tm wuerde Datum und
 * Uhrzeit in der fertigen Datei ueberschreiben. */
static struct tm START;
static uint64_t ERSTE_PROBE;

/* --- WAV mit BEXT ------------------------------------------------------
 * Reihenfolge: RIFF, fmt, bext, data. Der bext-Block muss vor data
 * stehen, sonst finden ihn manche Schnittprogramme nicht.
 */
#pragma pack(push, 1)
struct fmt_chunk {
    char     id[4];
    uint32_t groesse;
    uint16_t format;         /* 1 = PCM */
    uint16_t kanaele;
    uint32_t rate;
    uint32_t bytes_je_sek;
    uint16_t block;
    uint16_t bits;
};
struct bext_chunk {
    char     id[4];
    uint32_t groesse;
    char     beschreibung[256];
    char     urheber[32];
    char     referenz[32];
    char     datum[10];      /* JJJJ-MM-TT */
    char     zeit[8];        /* HH:MM:SS */
    uint32_t zeitbezug_lo;   /* Samples seit Mitternacht */
    uint32_t zeitbezug_hi;
    uint16_t version;
    uint8_t  umid[64];
    uint16_t loudness[5];
    uint8_t  reserve[180];
};
#pragma pack(pop)

static esp_err_t kopf_schreiben(const struct tm *start, uint64_t erste_probe,
                                uint32_t daten_bytes)
{
    const uint32_t bext_nutz = sizeof(struct bext_chunk) - 8;
    const uint32_t fmt_nutz = sizeof(struct fmt_chunk) - 8;
    const uint32_t riff = 4 + (8 + fmt_nutz) + (8 + bext_nutz) + 8 + daten_bytes;

    if (fseek(WAV, 0, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    fwrite("RIFF", 1, 4, WAV);
    fwrite(&riff, 4, 1, WAV);
    fwrite("WAVE", 1, 4, WAV);

    struct fmt_chunk f = {
        .id = {'f', 'm', 't', ' '}, .groesse = fmt_nutz, .format = 1,
        .kanaele = AUDIO_KANAELE, .rate = AUDIO_RATE,
        .bytes_je_sek = AUDIO_RATE * AUDIO_BYTES_JE_RAHMEN,
        .block = AUDIO_BYTES_JE_RAHMEN, .bits = AUDIO_BITS,
    };
    fwrite(&f, sizeof f, 1, WAV);

    struct bext_chunk b;
    memset(&b, 0, sizeof b);
    memcpy(b.id, "bext", 4);
    b.groesse = bext_nutz;
    snprintf(b.beschreibung, sizeof b.beschreibung,
             "exhaust-mic, 2 Kanaele, %u Hz, %u Bit", AUDIO_RATE, AUDIO_BITS);
    snprintf(b.urheber, sizeof b.urheber, "exhaust-mic");
    /* Beide Felder sind ohne Nullbyte definiert: 10 bzw. 8 Zeichen.
     * strftime schreibt immer eine Null ans Ende, deshalb ueber einen
     * Zwischenpuffer - sonst laeuft das Datum in das Zeitfeld. */
    char zwischen[16];
    strftime(zwischen, sizeof zwischen, "%Y-%m-%d", start);
    memcpy(b.datum, zwischen, sizeof b.datum);
    strftime(zwischen, sizeof zwischen, "%H:%M:%S", start);
    memcpy(b.zeit, zwischen, sizeof b.zeit);
    b.zeitbezug_lo = (uint32_t)(erste_probe & 0xffffffffu);
    b.zeitbezug_hi = (uint32_t)(erste_probe >> 32);
    b.version = 1;
    fwrite(&b, sizeof b, 1, WAV);

    fwrite("data", 1, 4, WAV);
    fwrite(&daten_bytes, 4, 1, WAV);
    return ESP_OK;
}

esp_err_t speicher_start(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = PIN_SD_CLK;
    slot.cmd = PIN_SD_CMD;
    slot.d0 = PIN_SD_D0;
    slot.d1 = PIN_SD_D1;
    slot.d2 = PIN_SD_D2;
    slot.d3 = PIN_SD_D3;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 32 * 1024,
    };
    esp_err_t e = esp_vfs_fat_sdmmc_mount(WURZEL, &host, &slot, &mount, &KARTE);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Karte nicht eingebunden: %s", esp_err_to_name(e));
        return e;
    }
    ESP_LOGI(TAG, "Karte %s, %llu MB", KARTE->cid.name,
             ((uint64_t)KARTE->csd.capacity * KARTE->csd.sector_size) >> 20);
    return ESP_OK;
}

bool speicher_bereit(void) { return KARTE != NULL; }
bool speicher_laeuft(void) { return WAV != NULL; }
uint64_t speicher_bytes(void) { return GESCHRIEBEN; }

esp_err_t speicher_datei_oeffnen(const struct tm *start, uint64_t erste_probe)
{
    if (WAV) {
        return ESP_ERR_INVALID_STATE;
    }
    char name[64], notiz[64];
    strftime(name, sizeof name, WURZEL "/%Y-%m-%d_%H%M%S.wav", start);
    strftime(notiz, sizeof notiz, WURZEL "/%Y-%m-%d_%H%M%S.csv", start);

    WAV = fopen(name, "wb");
    if (!WAV) {
        ESP_LOGE(TAG, "%s liess sich nicht anlegen", name);
        return ESP_FAIL;
    }
    setvbuf(WAV, NULL, _IOFBF, 32 * 1024);
    GESCHRIEBEN = 0;
    START = *start;
    ERSTE_PROBE = erste_probe;
    ESP_RETURN_ON_ERROR(kopf_schreiben(start, erste_probe, 0), TAG, "Kopf");

    CSV = fopen(notiz, "w");
    if (CSV) {
        fprintf(CSV, "sample,ereignis,wert\n");
    }
    ESP_LOGI(TAG, "Aufnahme in %s", name);
    return ESP_OK;
}

esp_err_t speicher_schreiben(const void *daten, size_t bytes)
{
    if (!WAV) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fwrite(daten, 1, bytes, WAV) != bytes) {
        ESP_LOGE(TAG, "Schreibfehler nach %llu Byte", GESCHRIEBEN);
        return ESP_FAIL;
    }
    GESCHRIEBEN += bytes;
    return ESP_OK;
}

esp_err_t speicher_datei_schliessen(void)
{
    if (!WAV) {
        return ESP_OK;
    }
    fflush(WAV);
    /* Laengen nachtragen. Erst jetzt sind sie bekannt. Datum, Uhrzeit und
     * Zeitbezug kommen unveraendert aus dem Anlegen zurueck. */
    kopf_schreiben(&START, ERSTE_PROBE, (uint32_t)GESCHRIEBEN);
    fclose(WAV);
    WAV = NULL;
    if (CSV) {
        fclose(CSV);
        CSV = NULL;
    }
    ESP_LOGI(TAG, "Aufnahme beendet, %llu Byte", GESCHRIEBEN);
    return ESP_OK;
}

esp_err_t speicher_notiz(const char *format, ...)
{
    if (!CSV) {
        return ESP_ERR_INVALID_STATE;
    }
    va_list ap;
    va_start(ap, format);
    vfprintf(CSV, format, ap);
    va_end(ap);
    fputc('\n', CSV);
    return ESP_OK;
}
