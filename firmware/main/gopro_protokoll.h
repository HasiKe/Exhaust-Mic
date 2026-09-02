/* Konstanten des Open-GoPro-Protokolls.
 *
 * Nachgeschlagen im offiziellen SDK, nicht aus dem Gedaechtnis:
 * demos/python/sdk_wireless_camera_control/open_gopro/models/constants/
 *   uuids.py     - Dienst- und Merkmalskennungen
 *   constants.py - CmdId, QueryCmdId
 *   statuses.py  - StatusId
 */

#pragma once

#include <stdint.h>

/* Dienst, mit dem sich die Kamera meldet. 16 Bit, wie beworben. */
#define GOPRO_DIENST_16       0xFEA6

/* Alle uebrigen Merkmale liegen auf b5f9XXXX-aa8d-11e3-9046-0002a5d5c51b */
#define GOPRO_MERKMAL_BEFEHL       0x0072
#define GOPRO_MERKMAL_BEFEHL_ANTW  0x0073
#define GOPRO_MERKMAL_ABFRAGE      0x0076
#define GOPRO_MERKMAL_ABFRAGE_ANTW 0x0077

/* Befehle auf dem Befehlsmerkmal */
#define GOPRO_CMD_AUSLOESER   0x01   /* SET_SHUTTER */

/* Befehle auf dem Abfragemerkmal */
#define GOPRO_Q_STATUS_LESEN  0x13   /* GET_STATUS_VAL */
#define GOPRO_Q_STATUS_AN     0x53   /* REG_STATUS_VAL_UPDATE */
#define GOPRO_Q_STATUS_AUS    0x73   /* UNREG_STATUS_VAL_UPDATE */
#define GOPRO_Q_STATUS_PUSH   0x93   /* STATUS_VAL_PUSH, kommt unaufgefordert */

/* Statuskennungen */
#define GOPRO_ST_BESCHAEFTIGT 8      /* BUSY */
#define GOPRO_ST_KODIERT      10     /* ENCODING - das ist unser Ausloeser */

/* Kopf einer Nachricht. Die oberen Bits des ersten Bytes sagen, wie die
 * Laenge kodiert ist; Fortsetzungspakete tragen Bit 7. Unsere Nachrichten
 * sind kurz, gelesen werden muss aber alles - die Kamera schickt beim
 * Anmelden alle angeforderten Werte auf einmal. */
#define GOPRO_KOPF_MASKE      0x60
#define GOPRO_KOPF_ALLGEMEIN  0x00   /* Laenge in den unteren 5 Bit */
#define GOPRO_KOPF_13BIT      0x20
#define GOPRO_KOPF_16BIT      0x40
#define GOPRO_KOPF_FORTSETZ   0x80
