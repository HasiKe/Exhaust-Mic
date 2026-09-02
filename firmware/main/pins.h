/* Erzeugt von firmware/tools/pins_aus_netzliste.py - nicht von Hand
 * aendern. Quelle ist der Schaltplan ueber output/main.net.
 *
 * Modul: ESP32-S3-WROOM-1-N8R2, 8 MB Flash, 2 MB PSRAM (quad).
 * Beim N8R2 sind GPIO35..37 frei - bei den Octal-PSRAM-Varianten
 * waeren sie belegt und die GPS-Stiftleiste unbrauchbar.
 */

#pragma once

#define PIN_I2S_BCK          5   /* Modulpin  5, Bittakt vom PCM1863, der ist Master */
#define PIN_I2S_LRCK         6   /* Modulpin  6, Wortakt vom PCM1863 */
#define PIN_I2S_DIN          7   /* Modulpin  7, Daten vom Wandler, aus Sicht des ESP32 Eingang */
#define PIN_PCM_INT         17   /* Modulpin 10, Sammelmeldung des PCM1863, u. a. Uebersteuerung */
#define PIN_I2C_SCL         18   /* Modulpin 11, gemeinsam: PCM1863 und DS3231 */
#define PIN_I2C_SDA          8   /* Modulpin 12 */
#define PIN_RTC_SQW         15   /* Modulpin  8, 1 Hz von der DS3231, latcht den Samplezaehler */
#define PIN_SD_CLK          14   /* Modulpin 22, SDMMC, 4 Bit */
#define PIN_SD_CMD          13   /* Modulpin 21 */
#define PIN_SD_D0           12   /* Modulpin 20 */
#define PIN_SD_D1           11   /* Modulpin 19 */
#define PIN_SD_D2           10   /* Modulpin 18 */
#define PIN_SD_D3            9   /* Modulpin 17 */
#define PIN_BTN_REC         21   /* Modulpin 23, Taster, gegen Masse */
#define PIN_BTN_MARK        47   /* Modulpin 24, Taster, gegen Masse */
#define PIN_LED_REC         48   /* Modulpin 25 */
#define PIN_LED_ERR         38   /* Modulpin 31 */
#define PIN_LED_SYNC        39   /* Modulpin 32, Blitz fuer den Videomarker */
#define PIN_BUZZ            40   /* Modulpin 33, Piezo ueber Q4 */
#define PIN_CAN_TX           4   /* Modulpin  4, an den SN65HVD230 */
#define PIN_CAN_RX          16   /* Modulpin  9 */
#define PIN_PGOOD           41   /* Modulpin 34, Sammelmeldung der Versorgung */
#define PIN_IGN_SENSE        1   /* Modulpin 39, Zuendung ueber Teiler, ADC1 */
#define PIN_VBAT_SENSE       2   /* Modulpin 38, Bordnetz ueber Teiler, ADC1 */
#define PIN_GPS_TX          35   /* Modulpin 28, Stiftleiste J6, unbestueckt */
#define PIN_GPS_RX          36   /* Modulpin 29 */
#define PIN_GPS_PPS         37   /* Modulpin 30 */
#define PIN_UART0_TX        43   /* Modulpin 37, Konsole */
#define PIN_UART0_RX        44   /* Modulpin 36 */
