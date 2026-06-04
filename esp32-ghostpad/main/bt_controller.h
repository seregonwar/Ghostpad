/*
MIT License

Copyright (c) 2026 Seregon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/**
 * @file bt_controller.h
 * @brief Single-header Bluetooth HID controller library for ESP32-DevKitC V4 (USB-C)
 *
 * Fornisce un layer di astrazione per controller Bluetooth HID su ESP32.
 * Attualmente supporta: Sony DualSense (PS5).
 * L'architettura è estensibile ad altri controller tramite la tabella dei driver.
 *
 * @section usage Utilizzo
 * In UN solo .c file del progetto definire l'implementazione:
 * @code
 *   #define BT_CONTROLLER_IMPL
 *   #include "bt_controller.h"
 * @endcode
 * In tutti gli altri file includere senza il define.
 *
 * @section idf_deps Dipendenze ESP-IDF (sdkconfig / CMakeLists.txt)
 *   CONFIG_BT_ENABLED=y
 *   CONFIG_BT_CLASSIC_ENABLED=y
 *   CONFIG_BT_HID_HOST_ENABLED=y  (componente esp_hid)
 *   CONFIG_BTDM_CTRL_MODE_BTDM=y  (dual-mode)
 *
 *   idf_component_register(... REQUIRES esp_hid bt nvs_flash)
 *
 * @section threading Thread-safety
 * Le callback vengono invocate dal task Bluedroid.
 * btc_state, latest input, auto-reconnect BDA e ring-buffer input sono protetti da mutex interno.
 * Le API pubbliche sono thread-safe eccetto btc_init() / btc_deinit()
 * che devono essere chiamate prima/dopo qualsiasi altro accesso.
 *
 * @version 1.0.0
 * @standard C11 + GNU extensions minimi (nessun VLA, nessuna ricorsione)
 * @target   ESP32-DevKitC V4 (ESP-IDF >= 5.1)
 *
 * DESIGN RATIONALE:
 *   - Single-header: zero build-system friction, easy drop-in.
 *   - Nessuna malloc/free: tutti i buffer sono statici a dimensione fissa.
 *   - Architettura driver-table: aggiungere un controller = aggiungere una riga.
 *   - I dati IMU sono int16_t con fattore di scala esplicito (no float nei path critici).
 *   - Output report costruito su stack a dimensione fissa e firmato CRC32 per BT.
 */

#ifndef BT_CONTROLLER_H
#define BT_CONTROLLER_H

/*===========================================================================
 * Sezione pubblica (sempre visibile)
 *===========================================================================*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---- Versione ----------------------------------------------------------- */
#define BTC_VERSION_MAJOR 1U
#define BTC_VERSION_MINOR 0U
#define BTC_VERSION_PATCH 1U

/* ---- Limiti configurabili (override via -DBTC_xxx=yyy) ------------------ */

/** Dimensione ring-buffer eventi di input (deve essere potenza di 2). */
#ifndef BTC_INPUT_RING_SIZE
#  define BTC_INPUT_RING_SIZE 8U
#endif

/** Timeout connessione in millisecondi. */
#ifndef BTC_CONNECT_TIMEOUT_MS
#  define BTC_CONNECT_TIMEOUT_MS 15000U
#endif

/** Timeout invio output report in millisecondi. */
#ifndef BTC_OUTPUT_TIMEOUT_MS
#  define BTC_OUTPUT_TIMEOUT_MS 50U
#endif

/** Livello di log (0=off, 1=error, 2=warn, 3=info, 4=debug). */
#ifndef BTC_LOG_LEVEL
#  define BTC_LOG_LEVEL 3
#endif

/* ---- Codici di errore --------------------------------------------------- */

/**
 * @brief Codici di ritorno delle API pubbliche.
 * Tutti i valori negativi indicano errore.
 */
typedef enum {
    BTC_OK                   =  0, /**< Operazione riuscita                     */
    BTC_ERR_INVALID_PARAM    = -1, /**< Parametro NULL o fuori range             */
    BTC_ERR_ALREADY_INIT     = -2, /**< btc_init() chiamato più volte            */
    BTC_ERR_NOT_INIT         = -3, /**< Libreria non inizializzata               */
    BTC_ERR_NOT_CONNECTED    = -4, /**< Nessun controller connesso               */
    BTC_ERR_BUFFER_FULL      = -5, /**< Ring-buffer eventi pieno                 */
    BTC_ERR_TIMEOUT          = -6, /**< Operazione scaduta                       */
    BTC_ERR_DRIVER           = -7, /**< Errore nel driver HID sottostante        */
    BTC_ERR_UNSUPPORTED      = -8, /**< Controller non supportato                */
    BTC_ERR_BUSY             = -9, /**< Risorsa occupata                         */
    BTC_ERR_NO_DATA          = -10,/**< Nessun dato disponibile                  */
} btc_err_t;

/* ---- Identificativo controller ------------------------------------------ */

/** Tipo di controller riconosciuto. */
typedef enum {
    BTC_CTRL_UNKNOWN   = 0,
    BTC_CTRL_DUALSENSE = 1,  /**< Sony DualSense (PS5) */
    /* --- Espansioni future --- */
    /* BTC_CTRL_DS4     = 2, */
    /* BTC_CTRL_XBOX    = 3, */
    BTC_CTRL_MAX_      = 16  /* Sentinella: non usare */
} btc_controller_type_t;

/* ---- Stato connessione --------------------------------------------------- */

typedef enum {
    BTC_STATE_IDLE        = 0, /**< In attesa                         */
    BTC_STATE_SCANNING    = 1, /**< Scansione dispositivi BT          */
    BTC_STATE_CONNECTING  = 2, /**< Handshake HID in corso            */
    BTC_STATE_CONNECTED   = 3, /**< Controller operativo              */
    BTC_STATE_RECONNECT   = 4, /**< Tentativo reconnessione           */
    BTC_STATE_DISCONNECTING = 5,
} btc_state_t;

/* ---- DPad --------------------------------------------------------------- */

/** Direzioni D-pad (codifica nibble DualSense 0x0..0x7, 0x8=rilasciato). */
typedef enum {
    BTC_DPAD_N    = 0,
    BTC_DPAD_NE   = 1,
    BTC_DPAD_E    = 2,
    BTC_DPAD_SE   = 3,
    BTC_DPAD_S    = 4,
    BTC_DPAD_SW   = 5,
    BTC_DPAD_W    = 6,
    BTC_DPAD_NW   = 7,
    BTC_DPAD_NONE = 8  /**< Nessuna direzione premuta */
} btc_dpad_t;

/* ---- Dati IMU ----------------------------------------------------------- */

/**
 * @brief Dati grezzi giroscopio e accelerometro (int16_t, no float).
 *
 * Fattori di scala DualSense:
 *   Giroscopio:    1 LSB = 1/1024 deg/s  →  gradi/s = val / 1024.0f
 *   Accelerometro: 1 LSB = 1/8192 g      →  g        = val / 8192.0f
 *
 * @note Thread-safety: read-only dopo ricezione, safe da leggere senza lock.
 */
typedef struct {
    int16_t gyro_pitch;  /**< Asse X giroscopio (pitch)             */
    int16_t gyro_yaw;    /**< Asse Y giroscopio (yaw)               */
    int16_t gyro_roll;   /**< Asse Z giroscopio (roll)              */
    int16_t accel_x;     /**< Accelerometro X                       */
    int16_t accel_y;     /**< Accelerometro Y                       */
    int16_t accel_z;     /**< Accelerometro Z                       */
    uint32_t sensor_ts;  /**< Timestamp sensore (unità: 0.33 µs)    */
} btc_imu_t;

/* ---- Dati touchpad DualSense -------------------------------------------- */

/**
 * @brief Singolo punto di contatto touchpad.
 * Coordinate: X [0..1920], Y [0..1080].
 */
typedef struct {
    bool     active;  /**< true se il dito è sul pad              */
    uint8_t  id;      /**< ID tracking contatto (0..127)          */
    uint16_t x;       /**< Coordinata X [0..1920]                 */
    uint16_t y;       /**< Coordinata Y [0..1080]                 */
} btc_touch_point_t;

/* ---- Batteria ------------------------------------------------------------ */

typedef struct {
    uint8_t level_pct;  /**< Livello [0..100] (granularità 10%)   */
    bool    charging;   /**< true se in carica via USB            */
} btc_battery_t;

/* ---- Stato input completo ----------------------------------------------- */

/**
 * @brief Snapshot immutabile dello stato controller.
 *
 * Questo tipo è prodotto dalla libreria e consegnato alla callback
 * o recuperabile con btc_get_state().
 * Tutti i campi sono validi solo se il campo `valid` è true.
 *
 * @note Dimensioni assi: [0..255], centro nominale = 128.
 */
typedef struct {
    bool valid;  /**< false se il dato non è stato ancora ricevuto */

    /* Assi analogici */
    uint8_t lx;  /**< Stick sinistro X [0..255]   */
    uint8_t ly;  /**< Stick sinistro Y [0..255]   */
    uint8_t rx;  /**< Stick destro X [0..255]     */
    uint8_t ry;  /**< Stick destro Y [0..255]     */
    uint8_t l2;  /**< Trigger L2 analogico [0..255] */
    uint8_t r2;  /**< Trigger R2 analogico [0..255] */

    /* D-pad */
    btc_dpad_t dpad;

    /* Pulsanti face */
    bool btn_square;
    bool btn_cross;
    bool btn_circle;
    bool btn_triangle;

    /* Trigger digitali */
    bool btn_l1;
    bool btn_r1;
    bool btn_l2;       /**< Digitale (threshold 128) */
    bool btn_r2;       /**< Digitale (threshold 128) */

    /* Stick press */
    bool btn_l3;
    bool btn_r3;

    /* Meta */
    bool btn_options;
    bool btn_create;   /**< "Share" equivalente PS5 */
    bool btn_ps;
    bool btn_touchpad; /**< Click touchpad          */
    bool btn_mute;

    /* Touchpad */
    btc_touch_point_t touch[2];

    /* IMU */
    btc_imu_t imu;

    /* Batteria */
    btc_battery_t battery;

    /* Numero sequenza report */
    uint8_t seq_num;
} btc_input_state_t;

/* ---- Output: LED & Rumble ------------------------------------------------ */

/**
 * @brief Colore RGB lightbar DualSense.
 * Gamma [0..255] per canale.
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} btc_rgb_t;

/** Effetto trigger adattivo (riservato: output non inviato finché non verificato). */
typedef enum {
    BTC_TRIGGER_EFFECT_OFF      = 0x00,
    BTC_TRIGGER_EFFECT_FEEDBACK = 0x01, /**< Resistenza uniforme */
    BTC_TRIGGER_EFFECT_WEAPON   = 0x02, /**< Click a soglia      */
    BTC_TRIGGER_EFFECT_VIBRATION= 0x05, /**< Vibrazione trigger  */
} btc_trigger_effect_t;

/** Parametri per un singolo trigger adattivo. */
typedef struct {
    btc_trigger_effect_t effect;
    uint8_t start_pos;   /**< Posizione di inizio effetto [0..9]   */
    uint8_t end_pos;     /**< Posizione di fine effetto [0..9]     */
    uint8_t force;       /**< Forza/ampiezza effetto [0..255]      */
} btc_trigger_params_t;

/**
 * @brief Comando di output verso il controller.
 *
 * Inviato tramite btc_set_output(). Solo i campi con il relativo
 * flag `set_*` attivo vengono applicati; gli altri vengono ignorati.
 */
typedef struct {
    /* Rumble motors */
    bool    set_rumble;
    uint8_t rumble_large;  /**< Motore grande [0..255] */
    uint8_t rumble_small;  /**< Motore piccolo [0..255] */

    /* LED lightbar */
    bool      set_led;
    btc_rgb_t led_color;

    /* LED player (4 indicatori sotto il touchpad) */
    bool    set_player_led;
    uint8_t player_led_bitmask; /**< bit[3:0] = LED 1..4 */

    /* Trigger adattivi */
    bool               set_left_trigger;
    btc_trigger_params_t left_trigger;

    bool               set_right_trigger;
    btc_trigger_params_t right_trigger;

    /* Microfono LED */
    bool set_mic_led;
    bool mic_led_on;
} btc_output_t;

/* ---- Callback ------------------------------------------------------------ */

/**
 * @brief Callback invocata su ogni nuovo input report.
 *
 * Viene chiamata dal task Bluedroid: DEVE essere non-bloccante.
 * Copiare i dati necessari e segnalare il proprio task applicativo.
 *
 * @param[in] state  Puntatore al nuovo stato (valido solo per la durata della callback).
 * @param[in] user   Puntatore utente passato a btc_init().
 *
 * @note Thread-safety: NOT safe da chiamare internamente alla callback.
 * @note WCET: < 100 µs (copiare ~80 byte + notifica FreeRTOS)
 */
typedef void (*btc_input_cb_t)(const btc_input_state_t *state, void *user);

/**
 * @brief Callback invocata su cambio di stato connessione.
 *
 * @param[in] new_state  Nuovo stato.
 * @param[in] ctrl_type  Tipo controller (BTC_CTRL_UNKNOWN se non identificato).
 * @param[in] user       Puntatore utente.
 */
typedef void (*btc_conn_cb_t)(btc_state_t new_state,
                               btc_controller_type_t ctrl_type,
                               void *user);

/* ---- Configurazione init ------------------------------------------------- */

/**
 * @brief Configurazione passata a btc_init().
 * I campi facoltativi possono essere lasciati a zero/NULL.
 */
typedef struct {
    /**
     * Callback di input (facoltativa, può essere NULL).
     * Se NULL, usare btc_pop_input() per polling.
     */
    btc_input_cb_t on_input;

    /**
     * Callback cambio connessione (facoltativa).
     */
    btc_conn_cb_t on_connection;

    /** Puntatore utente passato alle callback. */
    void *user_data;

    /**
     * Se true, abilita la riconnessione automatica all'ultimo
     * controller connesso (indirizzo salvato in NVS).
     */
    bool auto_reconnect;

    /**
     * Timeout scansione in ms (0 = usa BTC_CONNECT_TIMEOUT_MS).
     * Se entro questo tempo non si trovano controller, btc_scan_start()
     * si ferma e notifica BTC_STATE_IDLE.
     */
    uint32_t scan_timeout_ms;
} btc_config_t;

/* ---- API pubblica -------------------------------------------------------- */

/**
 * @brief Inizializza lo stack Bluetooth e la libreria.
 *
 * Alloca le risorse interne, inizializza NVS (se non già fatto),
 * abilita Bluedroid HID Host.
 * Deve essere chiamata PRIMA di qualsiasi altra funzione.
 *
 * @param[in] cfg  Configurazione (può essere NULL per defaults).
 *
 * @return BTC_OK in caso di successo.
 * @return BTC_ERR_ALREADY_INIT se già inizializzata.
 * @return BTC_ERR_DRIVER in caso di errore ESP-IDF.
 *
 * @pre cfg non deve essere modificata dopo la chiamata.
 * @note Thread-safety: NOT thread-safe. Chiamare una sola volta.
 * @note WCET: ~500 ms (inizializzazione BT stack)
 */
btc_err_t btc_lib_init(const btc_config_t *cfg);

/**
 * @brief Deinizializza la libreria e libera le risorse BT.
 *
 * @return BTC_OK
 * @return BTC_ERR_NOT_INIT se non inizializzata.
 *
 * @note Thread-safety: NOT thread-safe. Non chiamare concurrent con altre API.
 */
btc_err_t btc_lib_deinit(void);

/**
 * @brief Avvia la scansione BT per controller HID.
 *
 * Porta lo stato a BTC_STATE_SCANNING. Al ritrovamento di un
 * dispositivo HID compatibile tenta la connessione automaticamente.
 *
 * @return BTC_OK
 * @return BTC_ERR_NOT_INIT
 * @return BTC_ERR_BUSY se già in scansione o connesso.
 *
 * @note Thread-safety: thread-safe.
 */
btc_err_t btc_scan_start(void);

/**
 * @brief Interrompe la scansione BT.
 *
 * @return BTC_OK
 * @return BTC_ERR_NOT_INIT
 *
 * @note Thread-safety: thread-safe.
 */
btc_err_t btc_scan_stop(void);

/**
 * @brief Disconnette il controller attualmente connesso.
 *
 * @return BTC_OK
 * @return BTC_ERR_NOT_CONNECTED
 *
 * @note Thread-safety: thread-safe.
 */
btc_err_t btc_disconnect(void);

/**
 * @brief Recupera lo stato di input più recente (polling alternativo alla callback).
 *
 * @param[out] out  Buffer in cui copiare lo stato. Non può essere NULL.
 *
 * @return BTC_OK
 * @return BTC_ERR_INVALID_PARAM se out è NULL.
 * @return BTC_ERR_NOT_CONNECTED se nessun controller connesso.
 *
 * @note Thread-safety: thread-safe.
 */
btc_err_t btc_get_state(btc_input_state_t *out);

/**
 * @brief Preleva il primo evento dal ring-buffer (FIFO, alternativo alla callback).
 *
 * @param[out] out  Buffer di output. Non può essere NULL.
 *
 * @return BTC_OK se un evento era disponibile.
 * @return BTC_ERR_INVALID_PARAM se out è NULL.
 * @return BTC_ERR_NO_DATA se il buffer era vuoto (nessun evento pendente).
 *
 * @note Thread-safety: thread-safe.
 */
btc_err_t btc_pop_input(btc_input_state_t *out);

/**
 * @brief Invia un comando di output al controller connesso.
 *
 * I campi con il relativo flag `set_*` a false vengono ignorati.
 * Il report viene inviato in modo asincrono; la funzione ritorna
 * subito e non garantisce la ricezione da parte del controller.
 *
 * @param[in] output  Comando di output. Non può essere NULL.
 *
 * @return BTC_OK
 * @return BTC_ERR_INVALID_PARAM se output è NULL.
 * @return BTC_ERR_NOT_CONNECTED
 * @return BTC_ERR_UNSUPPORTED se il controller connesso non supporta output.
 *
 * @note Thread-safety: thread-safe.
 * @note WCET: < 200 µs (costruzione report + push HID queue)
 */
btc_err_t btc_set_output(const btc_output_t *output);

/**
 * @brief Restituisce il tipo di controller attualmente connesso.
 *
 * @return BTC_CTRL_UNKNOWN se non connesso.
 */
btc_controller_type_t btc_get_controller_type(void);

/**
 * @brief Restituisce lo stato corrente della connessione.
 */
btc_state_t btc_get_connection_state(void);

/**
 * @brief Versione in formato stringa (es. "1.0.0").
 */
const char *btc_version_string(void);

/*===========================================================================
 * Sezione implementazione (visibile solo se BT_CONTROLLER_IMPL definito)
 *===========================================================================*/
#ifdef BT_CONTROLLER_IMPL

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hid_common.h"
#include "esp_hidh.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <inttypes.h>
#include <stddef.h>

/* ---- Tag di log ESP-IDF ------------------------------------------------- */
static const char *BTC_TAG = "bt_ctrl";

/* ---- Macro di log condizionale ------------------------------------------ */

#if BTC_LOG_LEVEL >= 1
#  define BTC_LOGE(...) ESP_LOGE(BTC_TAG, __VA_ARGS__)
#else
#  define BTC_LOGE(...) ((void)0)
#endif

#if BTC_LOG_LEVEL >= 2
#  define BTC_LOGW(...) ESP_LOGW(BTC_TAG, __VA_ARGS__)
#else
#  define BTC_LOGW(...) ((void)0)
#endif

#if BTC_LOG_LEVEL >= 3
#  define BTC_LOGI(...) ESP_LOGI(BTC_TAG, __VA_ARGS__)
#else
#  define BTC_LOGI(...) ((void)0)
#endif

#if BTC_LOG_LEVEL >= 4
#  define BTC_LOGD(...) ESP_LOGD(BTC_TAG, __VA_ARGS__)
#else
#  define BTC_LOGD(...) ((void)0)
#endif

/* ---- Asserzioni di configurazione --------------------------------------- */

_Static_assert((BTC_INPUT_RING_SIZE & (BTC_INPUT_RING_SIZE - 1U)) == 0U,
               "BTC_INPUT_RING_SIZE deve essere potenza di 2");
_Static_assert(BTC_INPUT_RING_SIZE >= 2U,
               "BTC_INPUT_RING_SIZE deve essere almeno 2");

/* ---- VID/PID tabella riconoscimento ------------------------------------- */

/** Coppia VID/PID per identificare il controller. */
typedef struct {
    uint16_t               vid;
    uint16_t               pid;
    btc_controller_type_t  type;
    const char            *name;
} btc_vid_pid_entry_t;

static const btc_vid_pid_entry_t s_vid_pid_table[] = {
    { 0x054C, 0x0CE6, BTC_CTRL_DUALSENSE, "DualSense"            },
    { 0x054C, 0x0DF2, BTC_CTRL_DUALSENSE, "DualSense Edge"       },
    /* --- Espandere qui per nuovi controller --- */
};

#define BTC_VID_PID_TABLE_LEN \
    (sizeof(s_vid_pid_table) / sizeof(s_vid_pid_table[0]))

/* ---- Struttura interna stato --------------------------------------------- */

/**
 * @brief Stato globale della libreria.
 *
 * Un'unica istanza statica. Protetta da s_mutex dove indicato.
 */
typedef struct {
    bool              initialized;
    btc_config_t      cfg;
    btc_state_t       conn_state;            /**< Protetto da s_mutex */
    btc_controller_type_t ctrl_type;         /**< Protetto da s_mutex */

    esp_hidh_dev_t   *hid_dev;               /**< Protetto da s_mutex */

    /* Stato input più recente */
    btc_input_state_t latest;                /**< Protetto da s_mutex */

    /* Ring buffer input (lock-free tramite head/tail atomici)
     * NOTA: su ESP32 non esiste stdatomic con FreeRTOS senza
     * abilitare C11 atomics in idf; usiamo il mutex per semplicità
     * e correttezza garantita. */
    btc_input_state_t ring[BTC_INPUT_RING_SIZE];
    uint32_t          ring_head;             /**< Protetto da s_mutex */
    uint32_t          ring_tail;             /**< Protetto da s_mutex */

    /* Output report DualSense Bluetooth */
    uint8_t  output_seq;                     /**< Protetto da s_mutex */

    /* Ultimo controller noto per auto-reconnect. */
    esp_bd_addr_t last_bda;                  /**< Protetto da s_mutex */
    bool          last_bda_valid;            /**< Protetto da s_mutex */

} btc_ctx_t;

static btc_ctx_t s_ctx;
static SemaphoreHandle_t s_mutex = NULL;

/* ---- Helper mutex ------------------------------------------------------- */

/** Acquisisce il mutex con timeout. Ritorna false in caso di timeout. */
static bool btc_lock(void)
{
    if (s_mutex == NULL) {
        return false;
    }
    return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(BTC_OUTPUT_TIMEOUT_MS))
           == pdTRUE;
}

static void btc_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

/* ---- Lookup VID/PID ----------------------------------------------------- */

/**
 * @brief Identifica il tipo controller da VID e PID.
 *
 * @param[in] vid  Vendor ID.
 * @param[in] pid  Product ID.
 * @return Tipo controller, BTC_CTRL_UNKNOWN se non riconosciuto.
 */
static btc_controller_type_t btc_identify(uint16_t vid, uint16_t pid)
{
    for (size_t i = 0U; i < BTC_VID_PID_TABLE_LEN; ++i) {
        if ((s_vid_pid_table[i].vid == vid) &&
            (s_vid_pid_table[i].pid == pid)) {
            return s_vid_pid_table[i].type;
        }
    }
    return BTC_CTRL_UNKNOWN;
}

/* ---- Parser DualSense --------------------------------------------------- */

/*
 * DualSense HID reports over Bluetooth:
 * - Report 0x01: report compatto BT, contiene assi/pulsanti base.
 * - Report 0x31: report completo BT da 78 byte inclusi report_id e CRC32.
 *
 * ESP-IDF esp_hidh espone il report_id separatamente in ESP_HIDH_INPUT_EVENT;
 * alcune versioni/trasporti possono però lasciare il report_id anche in data[0].
 * Il parser accetta entrambe le forme in modo difensivo.
 */

#define DS_INPUT_REPORT_SIMPLE_ID       0x01U
#define DS_INPUT_REPORT_SIMPLE_MIN_LEN  10U
#define DS_INPUT_REPORT_BT_ID           0x31U
#define DS_INPUT_REPORT_BT_TOTAL_LEN    78U
#define DS_INPUT_REPORT_BT_PAYLOAD_LEN  77U
#define DS_INPUT_COMMON_LEN             63U
#define DS_INPUT_COMMON_OFFSET_BT       2U
#define DS_INPUT_CRC_OFFSET_BT          74U

#define DS_OUTPUT_REPORT_BT_ID          0x31U
#define DS_OUTPUT_REPORT_BT_TOTAL_LEN   78U
#define DS_OUTPUT_REPORT_BT_PAYLOAD_LEN 77U
#define DS_OUTPUT_SEQ_TAG_OFFSET        1U
#define DS_OUTPUT_TAG_OFFSET            2U
#define DS_OUTPUT_COMMON_OFFSET_BT      3U
#define DS_OUTPUT_COMMON_LEN            47U
#define DS_OUTPUT_CRC_OFFSET_BT         74U
#define DS_OUTPUT_TAG_VALUE             0x10U

#define DS_CRC32_INPUT_SEED             0xA1U
#define DS_CRC32_OUTPUT_SEED            0xA2U

#define DS_OUTPUT_VALID_FLAG0_HAPTICS_SELECT                   0x02U
#define DS_OUTPUT_VALID_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE      0x01U
#define DS_OUTPUT_VALID_FLAG1_POWER_SAVE_CONTROL_ENABLE        0x02U
#define DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE          0x04U
#define DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE  0x10U
#define DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE    0x02U
#define DS_OUTPUT_VALID_FLAG2_COMPATIBLE_VIBRATION2            0x04U
#define DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE                  0x10U
#define DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT                     0x02U

#define BTC_NVS_NAMESPACE "bt_ctrl"
#define BTC_NVS_KEY_LAST_BDA "last_bda"

/* Macro CoD Bluetooth Classic: Major/Minor device class. */
#define BTC_COD_MAJOR_DEVICE(cod_)  (((cod_) >> 8U) & 0x1FU)
#define BTC_COD_MINOR_DEVICE(cod_)  (((cod_) >> 2U) & 0x3FU)

/**
 * @brief Decodifica un punto di contatto touchpad da 4 byte raw.
 *
 * Formato packed: [active:1][id:7][x_lo:8][x_hi:4|y_lo:4][y_hi:8]
 */
static btc_touch_point_t btc_ds_parse_touch(const uint8_t *b)
{
    /* b deve puntare a 4 byte validi; il caller garantisce i bounds. */
    btc_touch_point_t tp;
    tp.active = ((b[0] & 0x80U) == 0U); /* bit 7 = 0 → attivo */
    tp.id     = (uint8_t)(b[0] & 0x7FU);
    tp.x      = (uint16_t)(b[1] | ((uint16_t)(b[2] & 0x0FU) << 8U));
    tp.y      = (uint16_t)((b[2] >> 4U) | ((uint16_t)b[3] << 4U));
    return tp;
}

/**
 * @brief Legge un int16_t little-endian da due byte.
 *
 * @param[in] b  Puntatore ai due byte (validi per almeno 2 byte).
 * @return Valore int16_t.
 *
 * @pre b != NULL && b+1 accessibile
 */
static int16_t btc_read_le16s(const uint8_t *b)
{
    uint16_t u = (uint16_t)b[0] | ((uint16_t)b[1] << 8U);
    int16_t  s;
    /* Evita UB da type-punning: copia sicura. */
    (void)memcpy(&s, &u, sizeof(s));
    return s;
}

/**
 * @brief Legge un uint32_t little-endian da quattro byte.
 */
static uint32_t btc_read_le32u(const uint8_t *b)
{
    return (uint32_t)b[0]
         | ((uint32_t)b[1] << 8U)
         | ((uint32_t)b[2] << 16U)
         | ((uint32_t)b[3] << 24U);
}


/**
 * @brief Scrive un uint32_t little-endian in quattro byte.
 */
static void btc_write_le32u(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v & 0xFFU);
    b[1] = (uint8_t)((v >> 8U) & 0xFFU);
    b[2] = (uint8_t)((v >> 16U) & 0xFFU);
    b[3] = (uint8_t)((v >> 24U) & 0xFFU);
}

/**
 * @brief Aggiorna CRC32 little-endian Ethernet/ZIP, polinomio riflesso.
 *
 * Implementazione tableless: bounded, senza memoria dinamica
 * e senza tabella globale da 1 KiB.
 */
static uint32_t btc_crc32_le_update(uint32_t crc,
                                     const uint8_t *data,
                                     size_t len)
{
    if ((data == NULL) && (len != 0U)) {
        return crc;
    }

    for (size_t i = 0U; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)(0U - (crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return crc;
}

/**
 * @brief Calcola il CRC32 PlayStation BT con seed report-type.
 */
static uint32_t btc_ps_crc32(uint8_t seed,
                              const uint8_t *data,
                              size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    crc = btc_crc32_le_update(crc, &seed, 1U);
    crc = btc_crc32_le_update(crc, data, len);
    return ~crc;
}

/**
 * @brief Verifica il CRC32 PlayStation BT.
 */
static bool btc_ps_crc32_check(uint8_t seed,
                                const uint8_t *data,
                                size_t len_without_crc,
                                uint32_t expected_crc)
{
    if (data == NULL) {
        return false;
    }
    return btc_ps_crc32(seed, data, len_without_crc) == expected_crc;
}

/**
 * @brief Popola btc_input_state_t dalla sezione comune DualSense completa.
 *
 * @param[in]  data  Puntatore alla struct comune da 63 byte.
 * @param[in]  len   Lunghezza disponibile, deve essere >= 63.
 * @param[out] out   Stato popolato.
 */
static btc_err_t btc_ds_parse_common_input(const uint8_t *data,
                                            size_t len,
                                            btc_input_state_t *out)
{
    if ((data == NULL) || (out == NULL)) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (len < DS_INPUT_COMMON_LEN) {
        return BTC_ERR_DRIVER;
    }

    (void)memset(out, 0, sizeof(*out));

    out->lx = data[0];
    out->ly = data[1];
    out->rx = data[2];
    out->ry = data[3];
    out->l2 = data[4];
    out->r2 = data[5];
    out->seq_num = data[6];

    {
        const uint8_t b0 = data[7];
        const uint8_t dpad_val = (uint8_t)(b0 & 0x0FU);
        out->dpad = (dpad_val <= (uint8_t)BTC_DPAD_NW)
                    ? (btc_dpad_t)dpad_val
                    : BTC_DPAD_NONE;
        out->btn_square   = ((b0 & 0x10U) != 0U);
        out->btn_cross    = ((b0 & 0x20U) != 0U);
        out->btn_circle   = ((b0 & 0x40U) != 0U);
        out->btn_triangle = ((b0 & 0x80U) != 0U);
    }

    {
        const uint8_t b1 = data[8];
        out->btn_l1      = ((b1 & 0x01U) != 0U);
        out->btn_r1      = ((b1 & 0x02U) != 0U);
        out->btn_l2      = ((b1 & 0x04U) != 0U);
        out->btn_r2      = ((b1 & 0x08U) != 0U);
        out->btn_create  = ((b1 & 0x10U) != 0U);
        out->btn_options = ((b1 & 0x20U) != 0U);
        out->btn_l3      = ((b1 & 0x40U) != 0U);
        out->btn_r3      = ((b1 & 0x80U) != 0U);
    }

    {
        const uint8_t b2 = data[9];
        out->btn_ps       = ((b2 & 0x01U) != 0U);
        out->btn_touchpad = ((b2 & 0x02U) != 0U);
        out->btn_mute     = ((b2 & 0x04U) != 0U);
    }

    out->imu.gyro_pitch = btc_read_le16s(&data[14]);
    out->imu.gyro_yaw   = btc_read_le16s(&data[16]);
    out->imu.gyro_roll  = btc_read_le16s(&data[18]);
    out->imu.accel_x    = btc_read_le16s(&data[20]);
    out->imu.accel_y    = btc_read_le16s(&data[22]);
    out->imu.accel_z    = btc_read_le16s(&data[24]);
    out->imu.sensor_ts  = btc_read_le32u(&data[26]);

    out->touch[0] = btc_ds_parse_touch(&data[31]);
    out->touch[1] = btc_ds_parse_touch(&data[35]);

    {
        const uint8_t bat = data[52];
        const uint8_t lvl = (uint8_t)(bat & 0x0FU);
        const uint8_t charging = (uint8_t)((bat >> 4U) & 0x0FU);

        out->battery.level_pct = (lvl >= 10U) ? 100U : (uint8_t)((lvl * 10U) + 5U);
        out->battery.charging = (charging == 0x01U) || (charging == 0x02U);
    }

    out->valid = true;
    return BTC_OK;
}

/**
 * @brief Parsing report compatto DualSense Bluetooth 0x01.
 *
 * Questo report non contiene IMU/touchpad/batteria; tali campi restano a zero.
 */
static btc_err_t btc_ds_parse_simple_input(const uint8_t *data,
                                            size_t len,
                                            btc_input_state_t *out)
{
    if ((data == NULL) || (out == NULL)) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (len < DS_INPUT_REPORT_SIMPLE_MIN_LEN) {
        return BTC_ERR_DRIVER;
    }

    (void)memset(out, 0, sizeof(*out));

    out->lx = data[0];
    out->ly = data[1];
    out->rx = data[2];
    out->ry = data[3];
    out->l2 = data[4];
    out->r2 = data[5];
    out->seq_num = data[6];

    {
        const uint8_t b0 = data[7];
        const uint8_t dpad_val = (uint8_t)(b0 & 0x0FU);
        out->dpad = (dpad_val <= (uint8_t)BTC_DPAD_NW)
                    ? (btc_dpad_t)dpad_val
                    : BTC_DPAD_NONE;
        out->btn_square   = ((b0 & 0x10U) != 0U);
        out->btn_cross    = ((b0 & 0x20U) != 0U);
        out->btn_circle   = ((b0 & 0x40U) != 0U);
        out->btn_triangle = ((b0 & 0x80U) != 0U);
    }

    {
        const uint8_t b1 = data[8];
        out->btn_l1      = ((b1 & 0x01U) != 0U);
        out->btn_r1      = ((b1 & 0x02U) != 0U);
        out->btn_l2      = ((b1 & 0x04U) != 0U);
        out->btn_r2      = ((b1 & 0x08U) != 0U);
        out->btn_create  = ((b1 & 0x10U) != 0U);
        out->btn_options = ((b1 & 0x20U) != 0U);
        out->btn_l3      = ((b1 & 0x40U) != 0U);
        out->btn_r3      = ((b1 & 0x80U) != 0U);
    }

    {
        const uint8_t b2 = data[9];
        out->btn_ps       = ((b2 & 0x01U) != 0U);
        out->btn_touchpad = ((b2 & 0x02U) != 0U);
        out->btn_mute     = ((b2 & 0x04U) != 0U);
    }

    out->valid = true;
    return BTC_OK;
}

/**
 * @brief Parsing report DualSense con report_id esplicito.
 */
static btc_err_t btc_ds_parse_input(uint16_t report_id,
                                     const uint8_t *data,
                                     size_t len,
                                     btc_input_state_t *out)
{
    if ((data == NULL) || (out == NULL)) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (len == 0U) {
        return BTC_ERR_DRIVER;
    }

    if ((report_id == 0U) &&
        ((data[0] == DS_INPUT_REPORT_SIMPLE_ID) ||
         (data[0] == DS_INPUT_REPORT_BT_ID))) {
        report_id = data[0];
    }

    if (report_id == DS_INPUT_REPORT_SIMPLE_ID) {
        const uint8_t *payload = data;
        size_t payload_len = len;

        if ((payload_len > 0U) && (payload[0] == DS_INPUT_REPORT_SIMPLE_ID)) {
            payload = &payload[1];
            payload_len--;
        }
        return btc_ds_parse_simple_input(payload, payload_len, out);
    }

    if (report_id == DS_INPUT_REPORT_BT_ID) {
        uint8_t frame[DS_INPUT_REPORT_BT_TOTAL_LEN];
        const uint8_t *full = NULL;

        if ((len >= DS_INPUT_REPORT_BT_TOTAL_LEN) &&
            (data[0] == DS_INPUT_REPORT_BT_ID)) {
            full = data;
        } else if (len >= DS_INPUT_REPORT_BT_PAYLOAD_LEN) {
            frame[0] = DS_INPUT_REPORT_BT_ID;
            (void)memcpy(&frame[1], data, DS_INPUT_REPORT_BT_PAYLOAD_LEN);
            full = frame;
        } else {
            BTC_LOGW("DualSense 0x31: len %zu troppo corta", len);
            return BTC_ERR_DRIVER;
        }

        {
            const uint32_t report_crc = btc_read_le32u(&full[DS_INPUT_CRC_OFFSET_BT]);
            if (!btc_ps_crc32_check((uint8_t)DS_CRC32_INPUT_SEED,
                                    full,
                                    DS_INPUT_CRC_OFFSET_BT,
                                    report_crc)) {
                BTC_LOGW("DualSense 0x31: CRC input non valido");
                return BTC_ERR_DRIVER;
            }
        }

        return btc_ds_parse_common_input(&full[DS_INPUT_COMMON_OFFSET_BT],
                                         DS_INPUT_COMMON_LEN,
                                         out);
    }

    return BTC_ERR_UNSUPPORTED;
}

/* ---- Costruttore output report DualSense -------------------------------- */

/**
 * @brief Costruisce il report di output Bluetooth DualSense 0x31.
 *
 * Il report Bluetooth completo è lungo 78 byte inclusi report_id e CRC32.
 * ESP-IDF riceve report_id separato in esp_hidh_dev_output_set(), ma il CRC
 * deve essere calcolato sul frame completo che include il report_id.
 */
static btc_err_t btc_ds_build_output(uint8_t seq,
                                      const btc_output_t *cmd,
                                      uint8_t *buf,
                                      size_t buf_len,
                                      size_t *written)
{
    if ((cmd == NULL) || (buf == NULL) || (written == NULL)) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (buf_len < DS_OUTPUT_REPORT_BT_TOTAL_LEN) {
        return BTC_ERR_INVALID_PARAM;
    }

    if (cmd->set_left_trigger || cmd->set_right_trigger) {
        return BTC_ERR_UNSUPPORTED;
    }

    (void)memset(buf, 0, DS_OUTPUT_REPORT_BT_TOTAL_LEN);

    buf[0] = DS_OUTPUT_REPORT_BT_ID;
    buf[DS_OUTPUT_SEQ_TAG_OFFSET] = (uint8_t)((seq & 0x0FU) << 4U);
    buf[DS_OUTPUT_TAG_OFFSET] = DS_OUTPUT_TAG_VALUE;

    uint8_t *common = &buf[DS_OUTPUT_COMMON_OFFSET_BT];

    if (cmd->set_rumble) {
        common[0] |= DS_OUTPUT_VALID_FLAG0_HAPTICS_SELECT;
        common[38] |= DS_OUTPUT_VALID_FLAG2_COMPATIBLE_VIBRATION2;
        common[2] = cmd->rumble_small; /* motor_right / weak */
        common[3] = cmd->rumble_large; /* motor_left / strong */
    }

    if (cmd->set_led) {
        common[1] |= DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
        common[38] |= DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
        common[41] = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT;
        common[42] = 0x02U; /* brightness: medium */
        common[44] = cmd->led_color.r;
        common[45] = cmd->led_color.g;
        common[46] = cmd->led_color.b;
    }

    if (cmd->set_player_led) {
        common[1] |= DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
        common[43] = (uint8_t)(cmd->player_led_bitmask & 0x1FU);
    }

    if (cmd->set_mic_led) {
        common[1] |= DS_OUTPUT_VALID_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE;
        common[8] = cmd->mic_led_on ? 0x01U : 0x00U;

        common[1] |= DS_OUTPUT_VALID_FLAG1_POWER_SAVE_CONTROL_ENABLE;
        if (cmd->mic_led_on) {
            common[9] |= DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE;
        } else {
            common[9] &= (uint8_t)~DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE;
        }
    }

    {
        const uint32_t crc = btc_ps_crc32((uint8_t)DS_CRC32_OUTPUT_SEED,
                                          buf,
                                          DS_OUTPUT_CRC_OFFSET_BT);
        btc_write_le32u(&buf[DS_OUTPUT_CRC_OFFSET_BT], crc);
    }

    *written = DS_OUTPUT_REPORT_BT_TOTAL_LEN;
    return BTC_OK;
}

/* ---- Tabella dei driver -------------------------------------------------- */

/**
 * @brief Descriptor driver per un tipo di controller.
 *
 * DESIGN: separare parsing e costruzione output in funzioni per tipo
 * rende banale aggiungere nuovi controller senza modificare il core.
 */
typedef struct {
    btc_controller_type_t type;

    /** Parsing report di input.
     *  @param data  Payload (senza report_id).
     *  @param len   Lunghezza payload.
     *  @param out   Output.
     */
    btc_err_t (*parse_input)(uint16_t report_id,
                              const uint8_t *data,
                              size_t len,
                              btc_input_state_t *out);

    /** Costruzione report di output.
     *  @param cmd      Comando.
     *  @param buf      Buffer output (DS_OUTPUT_TOTAL_LEN byte).
     *  @param buf_len  Dimensione buffer.
     *  @param written  Byte scritti.
     */
    btc_err_t (*build_output)(uint8_t seq,
                               const btc_output_t *cmd,
                               uint8_t *buf, size_t buf_len,
                               size_t *written);
} btc_driver_t;

static const btc_driver_t s_drivers[] = {
    {
        .type         = BTC_CTRL_DUALSENSE,
        .parse_input  = btc_ds_parse_input,
        .build_output = btc_ds_build_output,
    },
    /* --- Aggiungere qui i driver futuri --- */
};

#define BTC_DRIVERS_LEN (sizeof(s_drivers) / sizeof(s_drivers[0]))

/**
 * @brief Cerca il driver per il tipo controller indicato.
 *
 * @param[in] type  Tipo controller.
 * @return Puntatore al driver, NULL se non trovato.
 */
static const btc_driver_t *btc_find_driver(btc_controller_type_t type)
{
    for (size_t i = 0U; i < BTC_DRIVERS_LEN; ++i) {
        if (s_drivers[i].type == type) {
            return &s_drivers[i];
        }
    }
    return NULL;
}


/* ---- Helper NVS / address / discovery ---------------------------------- */

static bool btc_bda_is_nonzero(const uint8_t *bda)
{
    if (bda == NULL) {
        return false;
    }
    for (size_t i = 0U; i < sizeof(esp_bd_addr_t); ++i) {
        if (bda[i] != 0U) {
            return true;
        }
    }
    return false;
}

static void btc_nvs_load_last_bda(void)
{
    nvs_handle_t h;
    size_t len = sizeof(esp_bd_addr_t);
    esp_bd_addr_t tmp;

    if (nvs_open(BTC_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    if ((nvs_get_blob(h, BTC_NVS_KEY_LAST_BDA, tmp, &len) == ESP_OK) &&
        (len == sizeof(esp_bd_addr_t)) &&
        btc_bda_is_nonzero(tmp)) {
        if (btc_lock()) {
            (void)memcpy(s_ctx.last_bda, tmp, sizeof(esp_bd_addr_t));
            s_ctx.last_bda_valid = true;
            btc_unlock();
        }
    }

    nvs_close(h);
}

static void btc_nvs_save_last_bda(const uint8_t *bda)
{
    nvs_handle_t h;

    if (!btc_bda_is_nonzero(bda)) {
        return;
    }
    if (nvs_open(BTC_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }

    if (nvs_set_blob(h, BTC_NVS_KEY_LAST_BDA, bda, sizeof(esp_bd_addr_t)) == ESP_OK) {
        (void)nvs_commit(h);
    }

    nvs_close(h);
}

static bool btc_mem_contains_ascii(const uint8_t *buf,
                                    size_t len,
                                    const char *needle)
{
    size_t needle_len;

    if ((buf == NULL) || (needle == NULL)) {
        return false;
    }

    needle_len = strlen(needle);
    if ((needle_len == 0U) || (len < needle_len)) {
        return false;
    }

    for (size_t i = 0U; i <= (len - needle_len); ++i) {
        if (memcmp(&buf[i], needle, needle_len) == 0) {
            return true;
        }
    }

    return false;
}

static bool btc_cod_is_gamepad(uint32_t cod)
{
    const uint32_t major = BTC_COD_MAJOR_DEVICE(cod);
    const uint32_t minor = BTC_COD_MINOR_DEVICE(cod);

    return (major == 0x05U) && ((minor == 0x01U) || (minor == 0x02U));
}

static btc_err_t btc_open_bda(const uint8_t *bda)
{
    esp_bd_addr_t bda_copy;
    esp_hidh_dev_t *opened;

    if (!btc_bda_is_nonzero(bda)) {
        return BTC_ERR_INVALID_PARAM;
    }

    (void)memcpy(bda_copy, bda, sizeof(esp_bd_addr_t));
    opened = esp_hidh_dev_open(bda_copy, ESP_HID_TRANSPORT_BT, 0U);
    if (opened == NULL) {
        return BTC_ERR_DRIVER;
    }

    if (!btc_lock()) {
        (void)esp_hidh_dev_close(opened);
        return BTC_ERR_TIMEOUT;
    }
    s_ctx.hid_dev = opened;
    s_ctx.conn_state = BTC_STATE_CONNECTING;
    btc_unlock();

    if (s_ctx.cfg.on_connection != NULL) {
        s_ctx.cfg.on_connection(BTC_STATE_CONNECTING,
                                BTC_CTRL_UNKNOWN,
                                s_ctx.cfg.user_data);
    }

    return BTC_OK;
}

/* ---- Ring-buffer helpers ------------------------------------------------- */

/** Ritorna il numero di elementi nel ring-buffer (con mutex già acquisito). */
static uint32_t btc_ring_used_nolock(void)
{
    return s_ctx.ring_head - s_ctx.ring_tail;
}

/** Aggiunge un elemento al ring (con mutex già acquisito).
 *  Se il buffer è pieno sovrascrive il più vecchio (lossy). */
static void btc_ring_push_nolock(const btc_input_state_t *s)
{
    const uint32_t mask = (uint32_t)(BTC_INPUT_RING_SIZE - 1U);
    const uint32_t used = btc_ring_used_nolock();

    if (used >= (uint32_t)BTC_INPUT_RING_SIZE) {
        /* Buffer pieno: avanza il tail per fare spazio (drop più vecchio) */
        s_ctx.ring_tail++;
        BTC_LOGW("Ring buffer pieno, evento più vecchio scartato");
    }

    const uint32_t idx = s_ctx.ring_head & mask;
    s_ctx.ring[idx] = *s;
    s_ctx.ring_head++;
}

/** Preleva un elemento dal ring (con mutex già acquisito).
 *  Ritorna false se vuoto. */
static bool btc_ring_pop_nolock(btc_input_state_t *out)
{
    if (btc_ring_used_nolock() == 0U) {
        return false;
    }

    const uint32_t mask = (uint32_t)(BTC_INPUT_RING_SIZE - 1U);
    const uint32_t idx  = s_ctx.ring_tail & mask;
    *out = s_ctx.ring[idx];
    s_ctx.ring_tail++;
    return true;
}

/* ---- Callback interna HID Host ------------------------------------------ */

/**
 * @brief Handler eventi esp_hidh.
 *
 * Chiamato dal task Bluedroid. Tutti gli accessi a s_ctx sono protetti
 * da btc_lock()/btc_unlock().
 *
 * @param[in] handler_args  Ignorato.
 * @param[in] base          Event base (ESP_HIDH_EVENTS).
 * @param[in] id            ID evento.
 * @param[in] event_data    Dati evento.
 *
 * @note Thread-safety: chiamato solo dal task Bluedroid.
 * @note NOT reentrant (il mutex non è ricorsivo).
 */
static void btc_hidh_event_handler(void            *handler_args,
                                    esp_event_base_t base,
                                    int32_t          id,
                                    void            *event_data)
{
    (void)handler_args;
    (void)base;

    if (event_data == NULL) {
        BTC_LOGW("Evento HID senza payload: %" PRId32, id);
        return;
    }

    switch ((esp_hidh_event_t)id) {

    case ESP_HIDH_OPEN_EVENT: {
        const esp_hidh_event_data_t *data = (const esp_hidh_event_data_t *)event_data;

        if ((data->open.status != ESP_OK) || (data->open.dev == NULL)) {
            BTC_LOGE("OPEN_EVENT errore: %d", data->open.status);
            if (!btc_lock()) { return; }
            s_ctx.conn_state = BTC_STATE_IDLE;
            s_ctx.hid_dev    = NULL;
            s_ctx.ctrl_type  = BTC_CTRL_UNKNOWN;
            btc_unlock();
            if (s_ctx.cfg.on_connection != NULL) {
                s_ctx.cfg.on_connection(BTC_STATE_IDLE,
                                        BTC_CTRL_UNKNOWN,
                                        s_ctx.cfg.user_data);
            }
            return;
        }

        const esp_hid_device_config_t *dev_cfg =
            esp_hidh_dev_config_get(data->open.dev);

        uint16_t vid = 0U;
        uint16_t pid = 0U;

        if (dev_cfg != NULL) {
            vid = (uint16_t)dev_cfg->vendor_id;
            pid = (uint16_t)dev_cfg->product_id;
        }

        const btc_controller_type_t type = btc_identify(vid, pid);
        const uint8_t *bda = esp_hidh_dev_bda_get(data->open.dev);

        BTC_LOGI("Controller connesso: VID=0x%04X PID=0x%04X tipo=%d",
                 (unsigned)vid, (unsigned)pid, (int)type);

        if (bda != NULL) {
            btc_nvs_save_last_bda(bda);
        }

        if (!btc_lock()) { return; }
        s_ctx.hid_dev    = data->open.dev;
        s_ctx.ctrl_type  = type;
        s_ctx.conn_state = BTC_STATE_CONNECTED;
        if (bda != NULL) {
            (void)memcpy(s_ctx.last_bda, bda, sizeof(esp_bd_addr_t));
            s_ctx.last_bda_valid = true;
        }
        btc_unlock();

        if (s_ctx.cfg.on_connection != NULL) {
            s_ctx.cfg.on_connection(BTC_STATE_CONNECTED,
                                    type,
                                    s_ctx.cfg.user_data);
        }
        break;
    }

    case ESP_HIDH_CLOSE_EVENT: {
        const esp_hidh_event_data_t *data = (const esp_hidh_event_data_t *)event_data;
        BTC_LOGI("Controller disconnesso");

        if (!btc_lock()) { return; }
        s_ctx.hid_dev    = NULL;
        s_ctx.ctrl_type  = BTC_CTRL_UNKNOWN;
        s_ctx.conn_state = BTC_STATE_IDLE;
        s_ctx.latest.valid = false;
        btc_unlock();

        if (data->close.dev != NULL) {
            (void)esp_hidh_dev_free(data->close.dev);
        }

        if (s_ctx.cfg.on_connection != NULL) {
            s_ctx.cfg.on_connection(BTC_STATE_IDLE,
                                    BTC_CTRL_UNKNOWN,
                                    s_ctx.cfg.user_data);
        }
        break;
    }

    case ESP_HIDH_INPUT_EVENT: {
        const esp_hidh_event_data_t *data = (const esp_hidh_event_data_t *)event_data;

        const uint8_t *payload = data->input.data;
        const size_t   plen    = (size_t)data->input.length;
        const uint16_t report_id = data->input.report_id;

        if ((payload == NULL) || (plen == 0U)) {
            BTC_LOGW("INPUT_EVENT: payload NULL o vuoto");
            return;
        }

        if (!btc_lock()) { return; }
        const btc_controller_type_t type = s_ctx.ctrl_type;
        btc_unlock();

        const btc_driver_t *drv = btc_find_driver(type);
        if ((drv == NULL) || (drv->parse_input == NULL)) {
            BTC_LOGD("Nessun parser input per tipo %d", (int)type);
            return;
        }

        btc_input_state_t parsed;
        const btc_err_t err = drv->parse_input(report_id, payload, plen, &parsed);

        if (err != BTC_OK) {
            BTC_LOGW("Parsing input fallito: %d report_id=0x%02X len=%zu",
                     (int)err,
                     (unsigned)report_id,
                     plen);
            return;
        }

        btc_input_cb_t  cb    = NULL;
        void           *udata = NULL;

        if (!btc_lock()) { return; }
        s_ctx.latest = parsed;
        btc_ring_push_nolock(&parsed);
        cb    = s_ctx.cfg.on_input;
        udata = s_ctx.cfg.user_data;
        btc_unlock();

        if (cb != NULL) {
            cb(&parsed, udata);
        }
        break;
    }

    case ESP_HIDH_FEATURE_EVENT:
        BTC_LOGD("FEATURE_EVENT ricevuto");
        break;

    default:
        BTC_LOGD("Evento HID non gestito: %" PRId32, id);
        break;
    }
}

/* ---- Callback GAP (scansione) ------------------------------------------ */

/**
 * @brief Handler GAP per scansione dispositivi BT Classic.
 *
 * Filtra i dispositivi con CoD "Gamepad" (0x002508) e avvia la
 * connessione HID al primo trovato.
 */
static void btc_gap_event_handler(esp_bt_gap_cb_event_t  event,
                                    esp_bt_gap_cb_param_t *param)
{
    BTC_LOGI("GAP cb: event=%d", (int)event);
    if (param == NULL) {
        BTC_LOGW("Evento GAP senza parametri: %d", (int)event);
        return;
    }

    /* Log ALL GAP events for diagnostics */
    if (event != ESP_BT_GAP_DISC_RES_EVT) {
        BTC_LOGI("GAP event: %d", (int)event);
    }

    switch (event) {

    case ESP_BT_GAP_DISC_RES_EVT: {
        const uint8_t *bda = param->disc_res.bda;
        uint32_t cod = 0U;
        bool has_cod = false;
        bool name_is_dualsense = false;
        char dev_name[64] = "";
        int rssi = 0;

        for (int i = 0; i < param->disc_res.num_prop; ++i) {
            const esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];

            if ((prop->type == ESP_BT_GAP_DEV_PROP_COD) &&
                (prop->val != NULL) &&
                (prop->len >= (int)sizeof(uint32_t))) {
                (void)memcpy(&cod, prop->val, sizeof(uint32_t));
                has_cod = true;
            }

            if ((prop->type == ESP_BT_GAP_DEV_PROP_EIR) &&
                (prop->val != NULL) &&
                (prop->len > 0)) {
                const size_t eir_len = (size_t)prop->len;
                if (btc_mem_contains_ascii((const uint8_t *)prop->val,
                                           eir_len,
                                           "DualSense")) {
                    name_is_dualsense = true;
                }
            }

            if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME && prop->val != NULL) {
                size_t n = prop->len < (int)sizeof(dev_name)-1 ? prop->len : (int)sizeof(dev_name)-1;
                memcpy(dev_name, prop->val, n);
                dev_name[n] = '\0';
            }

            if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI && prop->len >= 1 && prop->val != NULL) {
                rssi = (int8_t)((const uint8_t *)prop->val)[0];
            }
        }

        /* Log EVERY device found for diagnostics */
        BTC_LOGI("BT dev: " ESP_BD_ADDR_STR " CoD=0x%06lX RSSI=%d name=\"%s\"",
                 ESP_BD_ADDR_HEX(bda), (unsigned long)cod, rssi, dev_name);

        if (((has_cod && btc_cod_is_gamepad(cod)) || name_is_dualsense) &&
            btc_bda_is_nonzero(bda)) {
            BTC_LOGI("Controller candidato trovato: " ESP_BD_ADDR_STR,
                     ESP_BD_ADDR_HEX(bda));

            const esp_err_t cancel_err = esp_bt_gap_cancel_discovery();
            if (cancel_err != ESP_OK) {
                BTC_LOGW("cancel_discovery: %d", cancel_err);
            }

            const btc_err_t open_err = btc_open_bda(bda);
            if (open_err != BTC_OK) {
                BTC_LOGE("Connessione HID fallita: %d", (int)open_err);
                if (btc_lock()) {
                    s_ctx.conn_state = BTC_STATE_IDLE;
                    btc_unlock();
                }
                if (s_ctx.cfg.on_connection != NULL) {
                    s_ctx.cfg.on_connection(BTC_STATE_IDLE,
                                            BTC_CTRL_UNKNOWN,
                                            s_ctx.cfg.user_data);
                }
            }
        }
        break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            BTC_LOGI("Scansione terminata");

            if (!btc_lock()) { return; }
            const btc_state_t st = s_ctx.conn_state;
            btc_unlock();

            if (st == BTC_STATE_SCANNING) {
                if (!btc_lock()) { return; }
                s_ctx.conn_state = BTC_STATE_IDLE;
                btc_unlock();

                if (s_ctx.cfg.on_connection != NULL) {
                    s_ctx.cfg.on_connection(BTC_STATE_IDLE,
                                            BTC_CTRL_UNKNOWN,
                                            s_ctx.cfg.user_data);
                }
            }
        }
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat != ESP_BT_STATUS_SUCCESS) {
            BTC_LOGE("Autenticazione BT fallita: %d",
                     param->auth_cmpl.stat);
        } else {
            BTC_LOGI("Autenticazione BT completata: %s",
                     param->auth_cmpl.device_name);
        }
        break;

    default:
        break;
    }
}

/* ---- Implementazione API pubblica --------------------------------------- */

/**
 * @brief Stringa di versione libreria.
 */
const char *btc_version_string(void)
{
    return "1.0.1";
}

/**
 * @brief Inizializza lo stack Bluetooth e la libreria.
 *
 * @note Thread-safety: NOT thread-safe. Chiamare una sola volta da app_main().
 */
btc_err_t btc_lib_init(const btc_config_t *cfg)
{
    if (s_ctx.initialized) {
        return BTC_ERR_ALREADY_INIT;
    }

    /* Azzera tutto il contesto */
    (void)memset(&s_ctx, 0, sizeof(s_ctx));

    /* Applica configurazione (con defaults per campi zero) */
    if (cfg != NULL) {
        s_ctx.cfg = *cfg;
    }
    if (s_ctx.cfg.scan_timeout_ms == 0U) {
        s_ctx.cfg.scan_timeout_ms = BTC_CONNECT_TIMEOUT_MS;
    }

    /* Crea mutex */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        BTC_LOGE("Impossibile creare mutex");
        return BTC_ERR_DRIVER;
    }

    /* Inizializza NVS (necessario per BT) */
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }
    if (ret != ESP_OK) {
        BTC_LOGE("NVS init fallita: %d", ret);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return BTC_ERR_DRIVER;
    }

    /* Configura la memoria BT: disabilita BLE per risparmiare heap
     * se vogliamo solo Classic, altrimenti usare BTDM.
     * NOTA: DualSense usa BT Classic, non BLE. */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        BTC_LOGE("BT controller init fallita: %d", ret);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return BTC_ERR_DRIVER;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        BTC_LOGE("BT controller enable fallita: %d", ret);
        (void)esp_bt_controller_deinit();
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return BTC_ERR_DRIVER;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        BTC_LOGE("Bluedroid init fallita: %d", ret);
        (void)esp_bt_controller_disable();
        (void)esp_bt_controller_deinit();
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return BTC_ERR_DRIVER;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        BTC_LOGE("Bluedroid enable fallita: %d", ret);
        (void)esp_bluedroid_deinit();
        (void)esp_bt_controller_disable();
        (void)esp_bt_controller_deinit();
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return BTC_ERR_DRIVER;
    }

    /* Registra handler GAP */
    ret = esp_bt_gap_register_callback(btc_gap_event_handler);
    if (ret != ESP_OK) {
        BTC_LOGE("Registrazione GAP callback fallita: %d", ret);
        goto fail_bluedroid;
    }

    /* Modalità pairing: no pin (SSP), consenti connessioni in ingresso */
    esp_bt_sp_param_t sp_param = ESP_BT_SP_IOCAP_MODE;
    uint8_t iocap = ESP_BT_IO_CAP_NONE; /* No I/O: accetta pairing SSP */
    ret = esp_bt_gap_set_security_param(sp_param, &iocap, sizeof(iocap));
    if (ret != ESP_OK) {
        BTC_LOGE("Security param fallito: %d", ret);
        goto fail_bluedroid;
    }

    /* Imposta il device non discoverable finché non viene richiesta la scansione. */
    ret = esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    if (ret != ESP_OK) {
        BTC_LOGE("Scan mode init fallito: %d", ret);
        goto fail_bluedroid;
    }

    /* Inizializza HID Host */
    const esp_hidh_config_t hidh_cfg = {
        .callback       = btc_hidh_event_handler,
        .event_stack_size = 4096U,
        .callback_arg   = NULL,
    };
    ret = esp_hidh_init(&hidh_cfg);
    if (ret != ESP_OK) {
        BTC_LOGE("HID Host init fallita: %d", ret);
        goto fail_bluedroid;
    }

    s_ctx.initialized = true;
    s_ctx.conn_state  = BTC_STATE_IDLE;
    s_ctx.ctrl_type   = BTC_CTRL_UNKNOWN;

    btc_nvs_load_last_bda();

    BTC_LOGI("bt_controller v%s inizializzato", btc_version_string());
    return BTC_OK;

fail_bluedroid:
    (void)esp_bluedroid_disable();
    (void)esp_bluedroid_deinit();
    (void)esp_bt_controller_disable();
    (void)esp_bt_controller_deinit();
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
    return BTC_ERR_DRIVER;
}

/**
 * @brief Deinizializza la libreria.
 */
btc_err_t btc_lib_deinit(void)
{
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    (void)esp_hidh_deinit();
    (void)esp_bluedroid_disable();
    (void)esp_bluedroid_deinit();
    (void)esp_bt_controller_disable();
    (void)esp_bt_controller_deinit();

    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;

    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    BTC_LOGI("bt_controller deinizializzato");
    return BTC_OK;
}

/**
 * @brief Avvia la scansione BT.
 */
btc_err_t btc_scan_start(void)
{
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }
    const btc_state_t st = s_ctx.conn_state;
    const bool can_reconnect = s_ctx.cfg.auto_reconnect && s_ctx.last_bda_valid;
    esp_bd_addr_t reconnect_bda;
    if (can_reconnect) {
        (void)memcpy(reconnect_bda, s_ctx.last_bda, sizeof(esp_bd_addr_t));
    }
    btc_unlock();

    if ((st == BTC_STATE_SCANNING) ||
        (st == BTC_STATE_CONNECTING) ||
        (st == BTC_STATE_CONNECTED)) {
        return BTC_ERR_BUSY;
    }

    if (can_reconnect) {
        BTC_LOGI("Tentativo auto-reconnect a ultimo controller");
        const btc_err_t open_err = btc_open_bda(reconnect_bda);
        if (open_err == BTC_OK) {
            return BTC_OK;
        }
        BTC_LOGW("Auto-reconnect fallito, avvio scansione: %d", (int)open_err);
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }
    s_ctx.conn_state = BTC_STATE_SCANNING;
    btc_unlock();

    esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                             ESP_BT_GENERAL_DISCOVERABLE);
    if (err != ESP_OK) {
        BTC_LOGE("set_scan_mode fallita: %d", err);
        if (btc_lock()) {
            s_ctx.conn_state = BTC_STATE_IDLE;
            btc_unlock();
        }
        return BTC_ERR_DRIVER;
    }

    err = esp_bt_gap_start_discovery(
        ESP_BT_INQ_MODE_GENERAL_INQUIRY,
        30U, /* ~38 secondi */
        0);

    if (err != ESP_OK) {
        BTC_LOGE("start_discovery fallita: %d", err);
        if (btc_lock()) {
            s_ctx.conn_state = BTC_STATE_IDLE;
            btc_unlock();
        }
        return BTC_ERR_DRIVER;
    }

    if (s_ctx.cfg.on_connection != NULL) {
        s_ctx.cfg.on_connection(BTC_STATE_SCANNING,
                                BTC_CTRL_UNKNOWN,
                                s_ctx.cfg.user_data);
    }

    BTC_LOGI("Scansione avviata");
    return BTC_OK;
}

/**
 * @brief Interrompe la scansione BT.
 */
btc_err_t btc_scan_stop(void)
{
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    const esp_err_t err = esp_bt_gap_cancel_discovery();
    if (err != ESP_OK) {
        /* Potrebbe già essere ferma, non è un errore fatale */
        BTC_LOGW("cancel_discovery: %d", err);
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }
    if (s_ctx.conn_state == BTC_STATE_SCANNING) {
        s_ctx.conn_state = BTC_STATE_IDLE;
    }
    btc_unlock();

    return BTC_OK;
}

/**
 * @brief Disconnette il controller.
 */
btc_err_t btc_disconnect(void)
{
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }

    esp_hidh_dev_t *dev = s_ctx.hid_dev;

    if (dev == NULL) {
        btc_unlock();
        return BTC_ERR_NOT_CONNECTED;
    }

    s_ctx.conn_state = BTC_STATE_DISCONNECTING;
    btc_unlock();

    const esp_err_t err = esp_hidh_dev_close(dev);
    if (err != ESP_OK) {
        if (btc_lock()) {
            if (s_ctx.hid_dev == dev) {
                s_ctx.conn_state = BTC_STATE_CONNECTED;
            }
            btc_unlock();
        }
        BTC_LOGE("esp_hidh_dev_close fallita: %d", err);
        return BTC_ERR_DRIVER;
    }

    return BTC_OK;
}

/**
 * @brief Recupera lo stato di input più recente (polling).
 */
btc_err_t btc_get_state(btc_input_state_t *out)
{
    if (out == NULL) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }

    if (s_ctx.conn_state != BTC_STATE_CONNECTED) {
        btc_unlock();
        return BTC_ERR_NOT_CONNECTED;
    }

    *out = s_ctx.latest;
    btc_unlock();
    return BTC_OK;
}

/**
 * @brief Preleva il primo evento dal ring-buffer.
 */
btc_err_t btc_pop_input(btc_input_state_t *out)
{
    if (out == NULL) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }

    const bool ok = btc_ring_pop_nolock(out);
    btc_unlock();

    return ok ? BTC_OK : BTC_ERR_NO_DATA;
}

/**
 * @brief Invia un comando di output al controller connesso.
 */
btc_err_t btc_set_output(const btc_output_t *output)
{
    if (output == NULL) {
        return BTC_ERR_INVALID_PARAM;
    }
    if (!s_ctx.initialized) {
        return BTC_ERR_NOT_INIT;
    }

    if (!btc_lock()) {
        return BTC_ERR_TIMEOUT;
    }

    if ((s_ctx.conn_state != BTC_STATE_CONNECTED) || (s_ctx.hid_dev == NULL)) {
        btc_unlock();
        return BTC_ERR_NOT_CONNECTED;
    }

    esp_hidh_dev_t *dev = s_ctx.hid_dev;
    const btc_controller_type_t type = s_ctx.ctrl_type;
    const btc_driver_t *drv = btc_find_driver(type);

    if ((drv == NULL) || (drv->build_output == NULL)) {
        btc_unlock();
        return BTC_ERR_UNSUPPORTED;
    }

    uint8_t buf[DS_OUTPUT_REPORT_BT_TOTAL_LEN];
    size_t written = 0U;
    const uint8_t seq = s_ctx.output_seq;

    const btc_err_t build_err = drv->build_output(seq,
                                                  output,
                                                  buf,
                                                  sizeof(buf),
                                                  &written);
    if (build_err != BTC_OK) {
        btc_unlock();
        return build_err;
    }

    if (written < 2U) {
        btc_unlock();
        return BTC_ERR_DRIVER;
    }

    if (!esp_hidh_dev_exists(dev)) {
        s_ctx.hid_dev = NULL;
        s_ctx.conn_state = BTC_STATE_IDLE;
        btc_unlock();
        return BTC_ERR_NOT_CONNECTED;
    }

    const esp_err_t esp_err = esp_hidh_dev_output_set(
        dev,
        0U,
        buf[0],
        &buf[1],
        written - 1U);

    if (esp_err == ESP_OK) {
        s_ctx.output_seq = (uint8_t)((s_ctx.output_seq + 1U) & 0x0FU);
    }

    btc_unlock();

    if (esp_err != ESP_OK) {
        BTC_LOGE("output_set fallita: %d", esp_err);
        return BTC_ERR_DRIVER;
    }

    return BTC_OK;
}

/**
 * @brief Tipo controller corrente.
 */
btc_controller_type_t btc_get_controller_type(void)
{
    if (!s_ctx.initialized) {
        return BTC_CTRL_UNKNOWN;
    }

    if (!btc_lock()) {
        return BTC_CTRL_UNKNOWN;
    }
    const btc_controller_type_t t = s_ctx.ctrl_type;
    btc_unlock();
    return t;
}

/**
 * @brief Stato connessione corrente.
 */
btc_state_t btc_get_connection_state(void)
{
    if (!s_ctx.initialized) {
        return BTC_STATE_IDLE;
    }

    if (!btc_lock()) {
        return BTC_STATE_IDLE;
    }
    const btc_state_t st = s_ctx.conn_state;
    btc_unlock();
    return st;
}

#endif /* BT_CONTROLLER_IMPL */
#endif /* BT_CONTROLLER_H */
