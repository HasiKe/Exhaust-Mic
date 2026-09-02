/* Siehe gopro.h. Rolle: Zentral (Client). Die Kamera ist das Peripheriegeraet. */

#include "gopro.h"
#include "gopro_protokoll.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "gopro";

/* b5f9XXXX-aa8d-11e3-9046-0002a5d5c51b, in der Byte-Reihenfolge, die
 * NimBLE erwartet: rueckwaerts. Nur die beiden Bytes an Position 12/13
 * unterscheiden die Merkmale. */
#define GOPRO_UUID128(kurz)                                                  \
    BLE_UUID128_INIT(0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0x46, 0x90,         \
                     0xe3, 0x11, 0x8d, 0xaa,                                 \
                     (uint8_t)((kurz) & 0xff), (uint8_t)((kurz) >> 8),       \
                     0xf9, 0xb5)

static const ble_uuid16_t  UUID_DIENST      = BLE_UUID16_INIT(GOPRO_DIENST_16);
static const ble_uuid128_t UUID_BEFEHL      = GOPRO_UUID128(GOPRO_MERKMAL_BEFEHL);
static const ble_uuid128_t UUID_BEFEHL_ANTW = GOPRO_UUID128(GOPRO_MERKMAL_BEFEHL_ANTW);
static const ble_uuid128_t UUID_ABFRAGE     = GOPRO_UUID128(GOPRO_MERKMAL_ABFRAGE);
static const ble_uuid128_t UUID_ABFRAGE_ANTW= GOPRO_UUID128(GOPRO_MERKMAL_ABFRAGE_ANTW);

#define SAMMLER_MAX 256

struct sammler {
    uint8_t  puffer[SAMMLER_MAX];
    uint16_t erwartet;      /* Nutzlaenge laut Kopf */
    uint16_t haben;
};

static struct {
    gopro_zustand_t     zustand;
    uint16_t            conn;
    uint16_t            h_befehl, h_befehl_antw;
    uint16_t            h_abfrage, h_abfrage_antw;
    uint16_t            ende_dienst;
    bool                nimmt_auf;
    gopro_aufnahme_cb_t cb;
    void               *benutzer;
    struct sammler      s_abfrage;
} G;

static void suche_starten(void);

/* ------------------------------------------------------------ Nachrichten */

/* Kopf auswerten. Rueckgabe: Anzahl der Kopfbytes, 0 bei Fortsetzung. */
static int kopf_laenge(const uint8_t *d, uint16_t n, uint16_t *nutz)
{
    if (n < 1) {
        return -1;
    }
    if (d[0] & GOPRO_KOPF_FORTSETZ) {
        return 0;                       /* Fortsetzungspaket */
    }
    switch (d[0] & GOPRO_KOPF_MASKE) {
    case GOPRO_KOPF_ALLGEMEIN:
        *nutz = d[0] & 0x1f;
        return 1;
    case GOPRO_KOPF_13BIT:
        if (n < 2) {
            return -1;
        }
        *nutz = (uint16_t)((d[0] & 0x1f) << 8 | d[1]);
        return 2;
    case GOPRO_KOPF_16BIT:
        if (n < 3) {
            return -1;
        }
        *nutz = (uint16_t)(d[1] << 8 | d[2]);
        return 3;
    default:
        return -1;
    }
}

/* Ein empfangenes Paket in den Sammler legen. Rueckgabe: true, wenn die
 * Nachricht vollstaendig ist. Die Kamera zerlegt laengere Antworten in
 * mehrere Pakete - beim Anmelden auf einen Status kommt die Antwort mit
 * dem aktuellen Wert schon mit, und die passt zwar noch in eines, aber
 * darauf sollte man sich nicht verlassen. */
static bool sammeln(struct sammler *s, const uint8_t *d, uint16_t n)
{
    uint16_t nutz = 0;
    int kopf = kopf_laenge(d, n, &nutz);
    if (kopf < 0) {
        s->haben = 0;
        return false;
    }
    if (kopf > 0) {
        s->haben = 0;
        s->erwartet = nutz;
        d += kopf;
        n -= kopf;
    } else {
        d += 1;                          /* Fortsetzungsbyte ueberspringen */
        n -= 1;
    }
    if (s->haben + n > SAMMLER_MAX) {
        ESP_LOGW(TAG, "Nachricht laenger als %d Byte, verworfen", SAMMLER_MAX);
        s->haben = 0;
        return false;
    }
    memcpy(s->puffer + s->haben, d, n);
    s->haben += n;
    return s->haben >= s->erwartet && s->erwartet > 0;
}

static void aufnahme_melden(bool laeuft)
{
    if (laeuft == G.nimmt_auf) {
        return;
    }
    G.nimmt_auf = laeuft;
    ESP_LOGI(TAG, "Kamera %s", laeuft ? "nimmt auf" : "steht");
    if (G.cb) {
        G.cb(laeuft, G.benutzer);
    }
}

/* Statusliste auswerten: je Eintrag Kennung, Laenge, Wert. */
static void status_liste(const uint8_t *d, uint16_t n)
{
    uint16_t i = 0;
    while (i + 2 <= n) {
        uint8_t kennung = d[i];
        uint8_t laenge  = d[i + 1];
        i += 2;
        if (i + laenge > n) {
            break;
        }
        if (kennung == GOPRO_ST_KODIERT && laenge >= 1) {
            aufnahme_melden(d[i] != 0);
        }
        i += laenge;
    }
}

static void abfrage_antwort(const uint8_t *d, uint16_t n)
{
    if (n < 1) {
        return;
    }
    uint8_t befehl = d[0];
    if (befehl == GOPRO_Q_STATUS_PUSH) {
        status_liste(d + 1, (uint16_t)(n - 1));
    } else if (befehl == GOPRO_Q_STATUS_AN || befehl == GOPRO_Q_STATUS_LESEN) {
        /* [Befehl][Fehlercode][Kennung][Laenge][Wert]... */
        if (n < 2) {
            return;
        }
        if (d[1] != 0) {
            ESP_LOGW(TAG, "Abfrage 0x%02x abgelehnt, Fehler %u", befehl, d[1]);
            return;
        }
        status_liste(d + 2, (uint16_t)(n - 2));
    }
}

/* ------------------------------------------------------------- Schreiben */

static int schreiben(uint16_t handle, const uint8_t *d, uint16_t n)
{
    if (G.zustand != GOPRO_BEREIT || !handle) {
        return BLE_HS_ENOTCONN;
    }
    return ble_gattc_write_flat(G.conn, handle, d, n, NULL, NULL);
}

esp_err_t gopro_ausloesen(bool starten)
{
    /* Laenge 3, Befehl 0x01, Parameterlaenge 1, Wert */
    const uint8_t nachricht[] = {0x03, GOPRO_CMD_AUSLOESER, 0x01,
                                 starten ? 0x01 : 0x00};
    int rc = schreiben(G.h_befehl, nachricht, sizeof nachricht);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

static void status_abonnieren(void)
{
    /* Laenge 2, Befehl 0x53, Statuskennung 10. Die Kamera antwortet mit
     * dem aktuellen Wert und schickt danach jede Aenderung von selbst. */
    const uint8_t nachricht[] = {0x02, GOPRO_Q_STATUS_AN, GOPRO_ST_KODIERT};
    int rc = schreiben(G.h_abfrage, nachricht, sizeof nachricht);
    if (rc != 0) {
        ESP_LOGE(TAG, "Statusanmeldung fehlgeschlagen, rc=%d", rc);
    }
}

/* --------------------------------------------------------- Merkmalsuche */

static int auf_deskriptor(uint16_t conn, const struct ble_gatt_error *fehler,
                          uint16_t chr_ende, const struct ble_gatt_dsc *dsc,
                          void *arg)
{
    uint16_t merkmal = (uint16_t)(uintptr_t)arg;
    if (fehler->status == 0 && dsc &&
        ble_uuid_u16(&dsc->uuid.u) == BLE_GATT_DSC_CLT_CFG_UUID16) {
        static const uint8_t an[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn, dsc->handle, an, sizeof an, NULL, NULL);
        ESP_LOGD(TAG, "Benachrichtigung fuer Merkmal %u eingeschaltet", merkmal);
    }
    if (fehler->status == BLE_HS_EDONE && merkmal == G.h_abfrage_antw) {
        /* Beide Abonnements stehen, jetzt anmelden. */
        G.zustand = GOPRO_BEREIT;
        status_abonnieren();
    }
    return 0;
}

static int auf_merkmal(uint16_t conn, const struct ble_gatt_error *fehler,
                       const struct ble_gatt_chr *chr, void *arg)
{
    if (fehler->status == BLE_HS_EDONE) {
        if (!G.h_befehl || !G.h_abfrage || !G.h_abfrage_antw) {
            ESP_LOGE(TAG, "Kamera meldet den Dienst, aber nicht alle Merkmale");
            ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        /* Benachrichtigungen einschalten: erst Befehls-, dann Abfrageantwort. */
        ble_gattc_disc_all_dscs(conn, G.h_befehl_antw, G.ende_dienst,
                                auf_deskriptor,
                                (void *)(uintptr_t)G.h_befehl_antw);
        ble_gattc_disc_all_dscs(conn, G.h_abfrage_antw, G.ende_dienst,
                                auf_deskriptor,
                                (void *)(uintptr_t)G.h_abfrage_antw);
        return 0;
    }
    if (fehler->status != 0 || !chr) {
        return 0;
    }
    if (ble_uuid_cmp(&chr->uuid.u, &UUID_BEFEHL.u) == 0) {
        G.h_befehl = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &UUID_BEFEHL_ANTW.u) == 0) {
        G.h_befehl_antw = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &UUID_ABFRAGE.u) == 0) {
        G.h_abfrage = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &UUID_ABFRAGE_ANTW.u) == 0) {
        G.h_abfrage_antw = chr->val_handle;
    }
    return 0;
}

static int auf_dienst(uint16_t conn, const struct ble_gatt_error *fehler,
                      const struct ble_gatt_svc *dienst, void *arg)
{
    if (fehler->status == 0 && dienst) {
        G.ende_dienst = dienst->end_handle;
        ble_gattc_disc_all_chrs(conn, dienst->start_handle, dienst->end_handle,
                                auf_merkmal, NULL);
    } else if (fehler->status == BLE_HS_EDONE && !G.ende_dienst) {
        ESP_LOGE(TAG, "Dienst 0x%04x nicht gefunden", GOPRO_DIENST_16);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

/* ------------------------------------------------------------------- GAP */

static bool wirbt_mit_dienst(const struct ble_hs_adv_fields *f)
{
    for (int i = 0; i < f->num_uuids16; i++) {
        if (ble_uuid_u16(&f->uuids16[i].u) == GOPRO_DIENST_16) {
            return true;
        }
    }
    return false;
}

static int gap_ereignis(struct ble_gap_event *e, void *arg)
{
    struct ble_hs_adv_fields felder;

    switch (e->type) {
    case BLE_GAP_EVENT_DISC:
        if (ble_hs_adv_parse_fields(&felder, e->disc.data,
                                    e->disc.length_data) != 0) {
            return 0;
        }
        if (!wirbt_mit_dienst(&felder)) {
            return 0;
        }
        ble_gap_disc_cancel();
        G.zustand = GOPRO_VERBINDET;
        ESP_LOGI(TAG, "Kamera gefunden, verbinde");
        if (ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &e->disc.addr, 30000, NULL,
                            gap_ereignis, NULL) != 0) {
            suche_starten();
        }
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status != 0) {
            ESP_LOGW(TAG, "Verbindung fehlgeschlagen, Status %d",
                     e->connect.status);
            suche_starten();
            return 0;
        }
        G.conn = e->connect.conn_handle;
        G.h_befehl = G.h_befehl_antw = G.h_abfrage = G.h_abfrage_antw = 0;
        G.ende_dienst = 0;
        ESP_LOGI(TAG, "verbunden, suche Dienst");
        ble_gattc_disc_svc_by_uuid(G.conn, &UUID_DIENST.u, auf_dienst, NULL);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Verbindung getrennt, Grund %d", e->disconnect.reason);
        /* Kein Stopp melden. Ein Funkabriss heisst nicht, dass die Kamera
         * aufgehoert hat - sie filmt weiter, und eine abgeschnittene
         * Tonspur waere der groessere Schaden. Die Aufnahme laeuft also
         * weiter, bis die Kamera es sagt oder jemand die Taste drueckt.
         *
         * Den gemerkten Zustand aber zuruecksetzen: nach dem
         * Wiederverbinden liefert die Anmeldung den wahren Wert, und
         * aufnahme_melden() meldet nur Aenderungen. Ohne das Zuruecksetzen
         * bliebe ein zwischenzeitlicher Stopp der Kamera unbemerkt. */
        G.nimmt_auf = false;
        G.zustand = GOPRO_SUCHT;
        suche_starten();
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint16_t n = OS_MBUF_PKTLEN(e->notify_rx.om);
        uint8_t  d[SAMMLER_MAX];
        if (n > sizeof d) {
            n = sizeof d;
        }
        if (ble_hs_mbuf_to_flat(e->notify_rx.om, d, n, &n) != 0) {
            return 0;
        }
        if (e->notify_rx.attr_handle == G.h_abfrage_antw) {
            if (sammeln(&G.s_abfrage, d, n)) {
                abfrage_antwort(G.s_abfrage.puffer, G.s_abfrage.haben);
                G.s_abfrage.haben = 0;
            }
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
        if (G.zustand == GOPRO_SUCHT) {
            suche_starten();            /* weitersuchen */
        }
        return 0;

    default:
        return 0;
    }
}

static void suche_starten(void)
{
    struct ble_gap_disc_params p = {
        .itvl = 0, .window = 0, .filter_policy = 0, .limited = 0,
        .passive = 0, .filter_duplicates = 1,
    };
    G.zustand = GOPRO_SUCHT;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &p, gap_ereignis, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Suche liess sich nicht starten, rc=%d", rc);
    }
}

static void bereit(void)
{
    ble_hs_util_ensure_addr(0);
    suche_starten();
}

static void host_task(void *arg)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t gopro_start(gopro_aufnahme_cb_t cb, void *benutzer)
{
    memset(&G, 0, sizeof G);
    G.cb = cb;
    G.benutzer = benutzer;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = bereit;
    ble_svc_gap_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set("exhaust-mic"));
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

gopro_zustand_t gopro_zustand(void) { return G.zustand; }
bool gopro_nimmt_auf(void) { return G.nimmt_auf; }
