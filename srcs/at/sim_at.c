/**
 * sim_at.c
 * Core AT engine implementation for SIMCom modem (ESP-IDF v5.3)
 *
 * NOTE: This implementation focuses on the core requested features. Higher-level
 * modules (SIM, MQTT, HTTP) are separate files (planned later).
 */

#include "at/sim_at.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "sim_at";

/* Internal configuration copy */
static bool g_debug = false;
static simcom_config_t* g_cfg;
static bool g_inited = false;

/* Parser task */
#define SIM_AT_PARSER_TASK_STACK 4096
#define SIM_AT_PARSER_TASK_PRIO 5
static TaskHandle_t s_parser_task = NULL;

/* UART parse response */
#define SIM_AT_MAX_LINES 5

static char s_responses[SIM_AT_MAX_LINES][SIM_AT_MAX_RESP_LEN];
static int s_resp_head = 0;  // write index
static int s_resp_tail = 0;  // read index
static int s_resp_count = 0; // number of stored responses

static char s_line_buf[SIM_AT_MAX_RESP_LEN];
static int s_line_pos = 0;

/* Last sent command — used to detect and discard echoed lines */
static char s_last_cmd[SIM_AT_MAX_CMD_LEN];

/* Modem reset flag — set when *ATREADY: 1 is received */
static volatile bool g_modem_reset = false;

/*
 * [RACE-FIX] s_sync_sem pasa de BINARIO a CONTADOR.
 *
 * El problema: los comandos con respuesta en dos fases (publish MQTT, NTP)
 * emiten "OK" y despues "+CMQTTPUB: 0,0". Las dos lineas suelen llegar en el
 * MISMO chunk de uart_read_bytes, asi que el parser hacia dos xSemaphoreGive()
 * seguidos. Un semaforo binario satura en 1 y el segundo give se PERDIA:
 * cmd_sync consumia el unico disponible y wait_resp se quedaba bloqueado hasta
 * el timeout, aunque la linea ya estuviera en el ring buffer.
 *
 * Consecuencia en produccion: el mensaje se publicaba bien, el caller creia que
 * habia fallado, lo guardaba en la FIFO y lo reenviaba mas tarde. Duplicados
 * que no venian ni del watchdog ni de la FIFO, mas el costo de un timeout
 * completo por cada mensaje.
 *
 * Con un contador, cada terminador queda encolado y ningun caller se pierde su
 * senal. El drenado al inicio de cada comando evita que se acumulen sobrantes.
 */
static SemaphoreHandle_t s_sync_sem = NULL;

/* [ASYNC-FIX] Tope de espera para simcom_wait_resp() cuando el caller pasa
 * portMAX_DELAY. Esperar de verdad para siempre con la radio prendida no es una
 * opcion en un equipo a bateria; ademas pdMS_TO_TICKS(0xFFFFFFFF) desborda. */
#define SIM_AT_WAIT_RESP_MAX_MS   60000U

/*
 * [RACE-FIX] Mutex del ring buffer y de s_last_cmd.
 *
 * s_resp_head / s_resp_tail / s_resp_count los escriben DOS tareas: el parser
 * (_add_resp_to_buff) y quien llame a simcom_get_resp() desde la main task.
 * s_resp_count++ contra s_resp_count-- no es atomico, y como el parser se crea
 * con xTaskCreate (sin afinidad) mientras main_task esta pineada al core 1,
 * pueden correr realmente en paralelo. Lo mismo con s_last_cmd, que escribe la
 * main task y lee el parser en _line_is_echo().
 *
 * Los holds son cortos y nunca envuelven un uart_write_bytes ni un bloqueo.
 */
static SemaphoreHandle_t s_buf_mutex = NULL;

#define BUF_LOCK()    do { if (s_buf_mutex) xSemaphoreTake(s_buf_mutex, portMAX_DELAY); } while (0)
#define BUF_UNLOCK()  do { if (s_buf_mutex) xSemaphoreGive(s_buf_mutex); } while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

void simcom_set_config(simcom_config_t* config)
{
    g_cfg = config;
}

void simcom_set_init_flag(bool init_f)
{
    g_inited = init_f;
}

simcom_err_t simcom_sem_create(void)
{
    /* [RACE-FIX] Contador, no binario. La capacidad cubre el peor caso de
     * terminadores encolados antes de que el caller los consuma. */
    s_sync_sem = xSemaphoreCreateCounting(SIM_AT_MAX_LINES, 0);
    if (!s_sync_sem)
        return SIM_AT_ERR_NO_MEM;

    s_buf_mutex = xSemaphoreCreateMutex(); // [RACE-FIX]
    if (!s_buf_mutex)
    {
        vSemaphoreDelete(s_sync_sem);
        s_sync_sem = NULL;
        return SIM_AT_ERR_NO_MEM;
    }

    return SIM_AT_OK;
}

void simcom_sem_delete(void)
{
    if (s_sync_sem)
    {
        vSemaphoreDelete(s_sync_sem);
        s_sync_sem = NULL;
    }
    if (s_buf_mutex) // [RACE-FIX]
    {
        vSemaphoreDelete(s_buf_mutex);
        s_buf_mutex = NULL;
    }
}

const char *simcom_err_to_str(simcom_err_t err)
{
    switch (err)
    {
    case SIM_AT_OK:                 return "SIM_AT_OK";
    case SIM_AT_ERR_INVALID_ARG:    return "SIM_AT_ERR_INVALID_ARG";
    case SIM_AT_ERR_NO_MEM:         return "SIM_AT_ERR_NO_MEM";
    case SIMCOM_ERR_TIMEOUT:        return "SIMCOM_ERR_TIMEOUT";
    case SIM_AT_ERR_UART:           return "SIM_AT_ERR_UART";
    case SIM_AT_ERR_BUSY:           return "SIM_AT_ERR_BUSY";
    case SIM_AT_ERR_INTERNAL:       return "SIM_AT_ERR_INTERNAL";
    case SIM_AT_ERR_NOT_INIT:       return "SIM_AT_ERR_NOT_INIT";
    case SIM_AT_ERR_OVERFLOW:       return "SIM_AT_ERR_OVERFLOW";
    case SIM_AT_ERR_ABORTED:        return "SIM_AT_ERR_ABORTED";
    case SIMCOM_ERR_MODEM_RESET:    return "SIMCOM_ERR_MODEM_RESET";
    default:                        return "INVALID ERR";
    }
    return "INVALID ERR";
}

const char* simcom_resp_err_to_str(simcom_responses_err_t err)
{
    switch (err)
    {
    case SIM_AT_RESPONSE_OK:                    return "SIM_AT_RESPONSE_OK";
    case SIM_AT_RESPONSE_COMMAND_OK:            return "SIM_AT_RESPONSE_COMMAND_OK";
    case SIM_AT_RESPONSE_ERR_INVALID_FORMAT:    return "SIM_AT_RESPONSE_ERR_INVALID_FORMAT";
    case SIM_AT_RESPONSE_ERR_COMMAND_ERROR:     return "SIM_AT_RESPONSE_ERR_COMMAND_ERROR";
    case SIM_AT_RESPONSE_ERR_COMMAND_INVALID:   return "SIM_AT_RESPONSE_ERR_COMMAND_INVALID";
    default:                                    return "INVALID ERR";
    }
    return "INVALID ERR";
}

/* ------------------------------------------------------------------ */
/* Internal debug helpers                                              */
/* ------------------------------------------------------------------ */

static void _print_bytes(uint8_t* data, int len)
{
    ESP_LOGI(TAG, "Received %d bytes:", len);
    for (int i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

static void _print_sent_command(const char* cmd, int len)
{
    char clean_cmd[SIM_AT_MAX_CMD_LEN];
    strncpy(clean_cmd, cmd, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';

    /* [BOUNDS-FIX] Antes: clean_cmd[len - 2] = '\0' sin chequear len.
     * Con un comando de 1 caracter (len = 1) eso escribia en clean_cmd[-1]. */
    int cut = len - 2;
    if (cut < 0)
        cut = 0;
    if (cut > (int)sizeof(clean_cmd) - 1)
        cut = (int)sizeof(clean_cmd) - 1;
    clean_cmd[cut] = '\0';

    ESP_LOGI(TAG, "--> %s", clean_cmd);
}

/* ------------------------------------------------------------------ */
/* Ring buffer                                                          */
/* ------------------------------------------------------------------ */

/* Llamar SIEMPRE con BUF_LOCK() tomado. */
static void _add_resp_to_buff_locked(const char* data)
{
    if (s_resp_count < SIM_AT_MAX_LINES)
    {
        strncpy(s_responses[s_resp_head], data, SIM_AT_MAX_RESP_LEN - 1);
        s_responses[s_resp_head][SIM_AT_MAX_RESP_LEN - 1] = '\0';
        s_resp_head = (s_resp_head + 1) % SIM_AT_MAX_LINES;
        s_resp_count++;
    }
    else
    {
        /* Buffer full: overwrite oldest entry and advance tail to keep
         * head and tail consistent. */
        strncpy(s_responses[s_resp_head], data, SIM_AT_MAX_RESP_LEN - 1);
        s_responses[s_resp_head][SIM_AT_MAX_RESP_LEN - 1] = '\0';
        s_resp_head = (s_resp_head + 1) % SIM_AT_MAX_LINES;
        s_resp_tail = (s_resp_tail + 1) % SIM_AT_MAX_LINES;
        /* s_resp_count stays at SIM_AT_MAX_LINES */
        ESP_LOGW(TAG, "Response ring buffer full — oldest entry overwritten");
    }
}

static void _add_resp_to_buff(const char* data)
{
    BUF_LOCK();                        // [RACE-FIX]
    _add_resp_to_buff_locked(data);
    BUF_UNLOCK();

    if (g_debug)
        ESP_LOGI(TAG, "<-- %s", data); // fuera del lock: ESP_LOGI puede bloquear
}

static void _reset_line_buff(void)
{
    s_line_pos = 0;
    s_line_buf[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Echo detection                                                       */
/* ------------------------------------------------------------------ */

static bool _line_is_echo(const char *line)
{
    char stripped[SIM_AT_MAX_CMD_LEN];

    BUF_LOCK(); // [RACE-FIX] s_last_cmd lo escribe la main task
    if (s_last_cmd[0] == '\0')
    {
        BUF_UNLOCK();
        return false;
    }
    strncpy(stripped, s_last_cmd, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';
    BUF_UNLOCK();

    int len = strlen(stripped);
    while (len > 0 && (stripped[len - 1] == '\r' || stripped[len - 1] == '\n'))
        stripped[--len] = '\0';

    return (strcmp(line, stripped) == 0);
}

/* ------------------------------------------------------------------ */
/* UART write                                                           */
/* ------------------------------------------------------------------ */

static simcom_err_t _prv_uart_write_cmd(const char *cmd)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;

    int len = strlen(cmd);

    BUF_LOCK(); // [RACE-FIX]
    strncpy(s_last_cmd, cmd, SIM_AT_MAX_CMD_LEN - 1);
    s_last_cmd[SIM_AT_MAX_CMD_LEN - 1] = '\0';
    BUF_UNLOCK();

    uart_wait_tx_done(g_cfg->uart_port, pdMS_TO_TICKS(100));
    int written = uart_write_bytes(g_cfg->uart_port, cmd, len);

    if (written != len)
        return SIM_AT_ERR_UART;

    if (g_debug) _print_sent_command(cmd, len);

    return SIM_AT_OK;
}

/* ------------------------------------------------------------------ */
/* Line classification                                                  */
/* ------------------------------------------------------------------ */

static bool _response_is_urc(const char *line)
{
    return (
        strstr(line, "+CGEV:")    != NULL ||
        strstr(line, "SMS")       != NULL ||
        strstr(line, "*ISIMAID")  != NULL ||
        strstr(line, "+SIMCARD:") != NULL ||
        strstr(line, "+MSTK:")    != NULL ||
        strstr(line, "PB DONE")   != NULL ||
        /* [URC-FIX] URCs que se colaban como lineas de datos y desincronizaban la
         * lectura de respuestas. Todos estos son PUROS (el modulo los emite solo, nunca
         * son respuesta a un comando nuestro), asi que descartarlos es seguro.
         * Zona horaria empujada por la red: */
        strstr(line, "+CTZV:")    != NULL ||
        strstr(line, "+CTZE:")    != NULL ||
        strstr(line, "PSUTTZ")    != NULL ||
        strstr(line, "DST:")      != NULL ||
        /* Indicadores y avisos varios: */
        strstr(line, "+CIEV:")    != NULL ||
        strstr(line, "+CMTI:")    != NULL ||
        strcmp(line, "RDY")       == 0    ||
        strstr(line, "NORMAL POWER DOWN") != NULL ||
        /* [URC-FIX] Aviso de red MQTT caida. Es URC puro (no lleva ':' ni es respuesta a
         * ningun comando) y se colaba en medio de otros: visto en el monitor
         * intercalandose en un AT+CFUN=0. No confundir con "+CMQTTCONNECT:" /
         * "+CMQTTPUB:", que SI son respuestas y no se filtran. */
        strcmp(line, "+CMQTTNONET") == 0
        /* OJO: "+CPIN:" NO se filtra aca a proposito, porque net_connect lo consulta
         * explicitamente con AT+CPIN?. Cuando llega como URC (tipico despues de
         * CFUN=1, con la SIM reinicializandose) lo absorbe el salteo de lineas de
         * simcom_resp_read_ok() / simcom_read_resp_values(). */
    );
}

static bool _response_is_modem_reset(const char *line)
{
    if (strstr(line, "*ATREADY: 1") != NULL)
    {
        g_modem_reset = true;
        ESP_LOGW(TAG, "Modem reset detected (*ATREADY: 1)");
        return true;
    }
    return false;
}

/**
 * @brief Returns true if this line marks the end of an AT response sequence.
 *
 * The SIMCom module always closes a response with one of:
 *   - "OK"        — command succeeded
 *   - "ERROR"     — command failed (plain or CME/CMS)
 *   - ">"         — module is prompting for more input (e.g. AT+CMGS)
 *
 * These are the ONLY lines that should unblock simcom_cmd_sync().
 * Intermediate data lines (+CSQ, +CREG, etc.) are accumulated silently.
 */
static bool _line_is_terminator(const char *line)
{
    /* Exact match for "OK" and "ERROR" to avoid false positives on
     * payload data that might contain those strings (e.g. a URL). */
    if (strcmp(line, "OK") == 0)
        return true;
    if (strcmp(line, "ERROR") == 0)
        return true;
    /* Extended error responses from AT+CMEE */
    if (strncmp(line, "+CME ERROR:", 11) == 0)
        return true;
    if (strncmp(line, "+CMS ERROR:", 11) == 0)
        return true;
    /* Prompt character — stored as a data line AND treated as a terminator */
    if (strcmp(line, ">") == 0)
        return true;

    return false;
}

/**
 * @brief Returns true if this line is a deferred async result that arrives
 * AFTER the initial OK, and needs to unblock a simcom_wait_resp() caller.
 *
 * These commands have a two-phase response:
 *   Phase 1: OK              — cmd_sync returns, caller calls wait_resp()
 *   Phase 2: +CNTP: <n>      — arrives seconds later, unblocks wait_resp()
 *
 * The line is stored in the ring buffer AND signals the semaphore, so the
 * caller can read it with simcom_get_resp() / simcom_read_resp_values().
 *
 * Add any other deferred-result prefixes here as they're discovered.
 */
static bool _line_is_async_terminator(const char *line)
{
    /* NTP sync result: +CNTP: <err_code> */
    if (strncmp(line, "+CNTP:", 6) == 0)
        return true;
    /* MQTT async events that arrive after the initial OK */
    if (strncmp(line, "+CMQTTCONNECT:", 14) == 0)
        return true;
    if (strncmp(line, "+CMQTTDISC:", 11) == 0)
        return true;
    if (strncmp(line, "+CMQTTPUB:", 10) == 0)
        return true;
    if (strncmp(line, "+CMQTTSUB:", 10) == 0)
        return true;

    return false;
}

/* ------------------------------------------------------------------ */
/* Parser task                                                          */
/* ------------------------------------------------------------------ */

static void _s_parser_task_fn(void *arg)
{
    const TickType_t rx_wait = pdMS_TO_TICKS(UART_MAX_WAITTIME);

    /* [LEAK-FIX] Antes esto era un malloc y el free() quedaba DESPUES del
     * while(1), o sea inalcanzable: cada simcom_parser_task_delete() perdia
     * SIM_AT_MAX_RESP_LEN + 1 bytes. Estatico y listo, la task es unica. */
    static uint8_t data[SIM_AT_MAX_RESP_LEN + 1];

    while (1)
    {
        int len = uart_read_bytes(g_cfg->uart_port, data, SIM_AT_MAX_RESP_LEN, rx_wait);
        if (len <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (g_debug) _print_bytes(data, len);

        for (int i = 0; i < len; i++)
        {
            char c = (char)data[i];

            /* ---- Accumulate into line buffer ---- */
            if (s_line_pos < SIM_AT_MAX_RESP_LEN - 1)
            {
                s_line_buf[s_line_pos++] = c;
                s_line_buf[s_line_pos]   = '\0';
            }

            /* ---- '>' prompt: treat immediately, don't wait for '\n' ---- */
            if (c == '>')
            {
                /* Store ">" as a data line so the caller can detect it */
                _add_resp_to_buff(">");
                /* ">" is also a terminator — wake up cmd_sync */
                xSemaphoreGive(s_sync_sem);
                _reset_line_buff();
                continue;
            }

            /* ---- End-of-line ---- */
            if (c == '\n')
            {
                /* Strip trailing CR/LF */
                while (s_line_pos > 0 &&
                       (s_line_buf[s_line_pos - 1] == '\r' ||
                        s_line_buf[s_line_pos - 1] == '\n'))
                {
                    s_line_buf[--s_line_pos] = '\0';
                }

                /* Discard blank lines */
                if (s_line_pos == 0)
                {
                    _reset_line_buff();
                    continue;
                }

                /* Discard echo */
                if (_line_is_echo(s_line_buf))
                {
                    if (g_debug)
                        ESP_LOGW(TAG, "Echo discarded: %s", s_line_buf);
                    _reset_line_buff();
                    continue;
                }

                /* Discard modem-reset URC */
                if (_response_is_modem_reset(s_line_buf))
                {
                    _reset_line_buff();
                    continue;
                }

                /* Discard other URCs */
                if (_response_is_urc(s_line_buf))
                {
                    if (g_debug)
                        ESP_LOGW(TAG, "URC discarded: %s", s_line_buf);
                    _reset_line_buff();
                    continue;
                }

                /*
                 * Store the line unconditionally (data lines AND terminators
                 * both go into the ring buffer so the caller can inspect them).
                 */
                _add_resp_to_buff(s_line_buf);

                /*
                 * Signal the semaphore when:
                 *
                 *  a) A standard terminator arrives (OK / ERROR / CME / CMS).
                 *     This unblocks simcom_cmd_sync() after a synchronous exchange.
                 *
                 *  b) A deferred async result arrives (+CNTP:, +CMQTTCONNECT:, ...).
                 *     These come AFTER the initial OK, seconds later.
                 *     simcom_wait_resp() blocks waiting for exactly this signal.
                 *
                 * [RACE-FIX] Con el semaforo contador, si "OK" y "+CMQTTPUB:"
                 * llegan en el mismo chunk los DOS gives quedan encolados y
                 * wait_resp ya no se pierde el segundo.
                 *
                 * Plain data lines (+CSQ:, +CREG:, +CGPADDR:, ...) are accumulated
                 * silently -- the caller reads them after cmd_sync returns.
                 */
                /* [ASYNC-FIX] SOLO los terminadores reales dan el semaforo.
                 *
                 * Antes las lineas diferidas (+CNTP:, +CMQTTPUB:, ...) tambien lo daban,
                 * aunque no hubiera nadie esperandolas. Caso real visto en el monitor:
                 * AT+CNTP? contesta "+CNTP: ...\r\nOK" y daba DOS senales; cmd_sync
                 * consumia una y la otra quedaba colgada. El comando siguiente
                 * (AT+CNTP="...") se la comia y volvia en 10 ms sin haber recibido un
                 * solo byte, con el ring buffer vacio. Lo mismo despues con AT+CFUN=0,
                 * que se comia el "+CNTP: 0" que llegaba tarde.
                 *
                 * Las lineas diferidas ahora solo se guardan en el ring buffer, y quien
                 * las espera las busca ahi con simcom_wait_resp(). */
                if (_line_is_terminator(s_line_buf))
                {
                    xSemaphoreGive(s_sync_sem);
                }

                _reset_line_buff();
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API — signatures unchanged                                    */
/* ------------------------------------------------------------------ */

simcom_err_t simcom_cmd_sync(const char *cmd, uint32_t timeout_ms)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;
    if (strlen(cmd) >= SIM_AT_MAX_CMD_LEN)
        return SIM_AT_ERR_INVALID_ARG;

    /* Reset the ring buffer so stale responses from a previous command
     * don't bleed into this one. */
    BUF_LOCK();                     // [RACE-FIX]
    s_resp_count = 0;
    s_resp_tail  = s_resp_head;
    BUF_UNLOCK();

    /* [RACE-FIX] El drenado va ANTES del write, no despues.
     *
     * Estaba despues: si el modulo contestaba rapido (un OK local llega en
     * pocos ms) el parser podia dar el semaforo antes de que corriera el
     * drenado, el drenado se comia la senal buena y cmd_sync esperaba hasta el
     * timeout con la respuesta ya guardada en el ring buffer. No es teorico:
     * main_task esta pineada al core 1 y el parser se crea sin afinidad. */
    while (xSemaphoreTake(s_sync_sem, 0) == pdTRUE)
        ;

    simcom_err_t r = _prv_uart_write_cmd(cmd);
    if (r != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error sending UART data");
        return r;
    }

    /*
     * Block until the parser signals a terminator (OK / ERROR / >).
     * All intermediate data lines have already been buffered by then.
     */
    TickType_t wait_ticks = pdMS_TO_TICKS(
        (timeout_ms == 0) ? g_cfg->default_cmd_timeout_ms : timeout_ms);

    if (xSemaphoreTake(s_sync_sem, wait_ticks) == pdFALSE)
        return SIMCOM_ERR_TIMEOUT;

    return SIM_AT_OK;
}

/* [ASYNC-FIX] Busca una linea en el ring buffer SIN consumirla: el caller la lee
 * despues con read_resp_values(). Con prefix == NULL busca cualquier linea diferida. */
static bool _buffer_has_line(const char *prefix)
{
    bool found = false;

    BUF_LOCK();
    for (int i = 0; i < s_resp_count; i++)
    {
        int idx = (s_resp_tail + i) % SIM_AT_MAX_LINES;

        if (prefix == NULL)
        {
            if (_line_is_async_terminator(s_responses[idx]))
            {
                found = true;
                break;
            }
        }
        else if (strncmp(s_responses[idx], prefix, strlen(prefix)) == 0)
        {
            found = true;
            break;
        }
    }
    BUF_UNLOCK();

    return found;
}

/**
 * [ASYNC-FIX] Espera una respuesta que llega DESPUES del OK inicial.
 *
 * Cubre dos usos distintos que conviven en la libreria:
 *
 *   a) Resultado diferido con prefijo propio (+CNTP:, +CMQTTPUB:, ...). Se detecta
 *      buscandolo en el ring buffer, porque el parser ya no da el semaforo para estas
 *      lineas (ver nota en la task del parser).
 *
 *   b) Un "OK" suelto que NO corresponde a ningun cmd_sync. Pasa con el prompt '>':
 *      AT+CMQTTTOPIC contesta '>', el caller escribe el dato crudo, y recien ahi llega
 *      el OK. Ese OK si da el semaforo, asi que se lo toma por esa via.
 *
 * Por eso se chequean LAS DOS cosas. Si solo se mirara el buffer de lineas diferidas,
 * cada topic y cada payload de MQTT daria timeout.
 *
 * Cuando se sabe que linea se espera conviene usar simcom_wait_resp_for(): es inmune a
 * que quede una linea diferida vieja dando vueltas en el buffer.
 */
simcom_err_t simcom_wait_resp(uint32_t timeout_ms)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;

    uint32_t t_ms = (timeout_ms == 0) ? g_cfg->default_cmd_timeout_ms : timeout_ms;

    /* [ASYNC-FIX] Tope duro: hay callers que pasan portMAX_DELAY. */
    if (t_ms > SIM_AT_WAIT_RESP_MAX_MS)
    {
        ESP_LOGW(TAG, "wait_resp: timeout %lu ms acotado a %u ms",
                 (unsigned long)t_ms, (unsigned)SIM_AT_WAIT_RESP_MAX_MS);
        t_ms = SIM_AT_WAIT_RESP_MAX_MS;
    }

    const TickType_t start = xTaskGetTickCount();
    const TickType_t limit = pdMS_TO_TICKS(t_ms);

    while (1)
    {
        if (_buffer_has_line(NULL))                     // caso (a)
            return SIM_AT_OK;

        if (xSemaphoreTake(s_sync_sem, 0) == pdTRUE)    // caso (b)
            return SIM_AT_OK;

        if ((xTaskGetTickCount() - start) >= limit)
            return SIMCOM_ERR_TIMEOUT;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * [ASYNC-FIX] Espera una linea concreta por prefijo (ej: "+CNTP:").
 *
 * Version estricta de wait_resp(): no puede confundirse ni con la senal de otro
 * comando ni con una linea diferida distinta que haya quedado en el buffer. Es la que
 * conviene usar en todo lo que tenga respuesta en dos fases.
 *
 * NO consume la linea: el caller la lee despues con simcom_read_resp_values().
 */
simcom_err_t simcom_wait_resp_for(const char *prefix, uint32_t timeout_ms)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;
    if (prefix == NULL || prefix[0] == '\0')
        return SIM_AT_ERR_INVALID_ARG;

    uint32_t t_ms = (timeout_ms == 0) ? g_cfg->default_cmd_timeout_ms : timeout_ms;

    if (t_ms > SIM_AT_WAIT_RESP_MAX_MS)
        t_ms = SIM_AT_WAIT_RESP_MAX_MS;

    const TickType_t start = xTaskGetTickCount();
    const TickType_t limit = pdMS_TO_TICKS(t_ms);

    while (1)
    {
        if (_buffer_has_line(prefix))
            return SIM_AT_OK;

        if ((xTaskGetTickCount() - start) >= limit)
        {
            ESP_LOGW(TAG, "wait_resp_for('%s'): timeout tras %lu ms",
                     prefix, (unsigned long)t_ms);
            return SIMCOM_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

simcom_err_t simcom_cmd_sync_ignore_resp(const char *cmd, uint32_t timeout_ms, uint8_t num_responses)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;
    if (strlen(cmd) >= SIM_AT_MAX_CMD_LEN)
        return SIM_AT_ERR_INVALID_ARG;

    /* [RACE-FIX] Esta variante no drenaba nada: arrastraba terminadores viejos
     * y podia "terminar" antes de tiempo. Mismo orden que cmd_sync. */
    while (xSemaphoreTake(s_sync_sem, 0) == pdTRUE)
        ;

    simcom_err_t r = _prv_uart_write_cmd(cmd);
    if (r != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error sending UART data");
        return r;
    }

    TickType_t wait_ticks = pdMS_TO_TICKS(
        (timeout_ms == 0) ? g_cfg->default_cmd_timeout_ms : timeout_ms);

    if (xSemaphoreTake(s_sync_sem, wait_ticks) == pdFALSE)
        return SIMCOM_ERR_TIMEOUT;

    for (int i = 0; i < num_responses; i++)
        simcom_ignore_resp();

    return SIM_AT_OK;
}

simcom_err_t simcom_uart_flush_rx(void)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;
    uart_flush_input(g_cfg->uart_port);
    return SIM_AT_OK;
}

bool simcom_get_resp(char *buf)
{
    BUF_LOCK(); // [RACE-FIX]

    if (s_resp_count == 0)
    {
        BUF_UNLOCK();
        return false;
    }

    strncpy(buf, s_responses[s_resp_tail], SIM_AT_MAX_RESP_LEN - 1);
    buf[SIM_AT_MAX_RESP_LEN - 1] = '\0';

    s_resp_tail = (s_resp_tail + 1) % SIM_AT_MAX_LINES;
    s_resp_count--;

    BUF_UNLOCK();
    return true;
}

void simcom_ignore_resp(void)
{
    BUF_LOCK(); // [RACE-FIX]

    if (s_resp_count == 0)
    {
        BUF_UNLOCK();
        return;
    }

    s_resp_tail = (s_resp_tail + 1) % SIM_AT_MAX_LINES;
    s_resp_count--;

    BUF_UNLOCK();
}

simcom_err_t simcom_enable_debug(bool en)
{
    g_debug = en;
    ESP_LOGI(TAG, "Debug %s", en ? "enable" : "disable");
    return SIM_AT_OK;
}

/**
 * [URC-FIX] Estas dos funciones leian UNA sola linea del ring buffer y decidian con
 * eso. Si un URC no filtrado se colaba entre el comando y su respuesta (el caso
 * tipico: "+CPIN: READY" despues de un AT+CFUN=1, con la SIM reinicializandose), la
 * linea leida era el URC y el comando se daba por invalido aunque el "OK" estuviera
 * en la linea siguiente del mismo buffer.
 *
 * Sintoma en campo: "Ok response was not received: SIM_AT_RESPONSE_ERR_COMMAND_INVALID"
 * en el primer comando de net_connect, y el ciclo entero sin red.
 *
 * Ahora saltean lineas hasta encontrar la que corresponde, y LOGUEAN lo que
 * descartaron, asi el proximo caso raro se diagnostica leyendo el monitor.
 *
 * Ademas: antes, si el buffer estaba vacio, simcom_get_resp() no tocaba 'resp' y se
 * decidia sobre basura del stack del caller. Ahora se limpia en cada vuelta.
 */
simcom_responses_err_t simcom_read_resp_values(char* resp, const char* key_word, char** index)
{
    for (int i = 0; i < SIM_AT_MAX_LINES; i++)
    {
        resp[0] = '\0';

        if (!simcom_get_resp(resp))
        {
            /* [LOG-NOISE] Nivel DEBUG, no WARN: hay callers que sondean esta funcion en
             * loop esperando una URC diferida, y con el buffer vacio en cada vuelta esto
             * llenaba la consola (el loop de publish llega a 100 iteraciones). Imprimir
             * a 115200 bloquea, o sea que el ruido se paga en tiempo de radio. */
            ESP_LOGD(TAG, "read_resp_values('%s'): buffer vacio", key_word);
            return SIM_AT_RESPONSE_ERR_COMMAND_INVALID;
        }

        if (strstr(resp, "ERROR") != NULL)
            return SIM_AT_RESPONSE_ERR_COMMAND_ERROR;

        if (strstr(resp, key_word) != NULL)
        {
            char *p = strchr(resp, ':');
            if (!p) return SIM_AT_RESPONSE_ERR_INVALID_FORMAT;
            while (*p == ':' || *p == ' ' || *p == '\t')
                p++;
            *index = p;
            return SIM_AT_RESPONSE_OK;
        }

        /* Comparacion exacta: antes un strstr(resp,"OK") daba falso positivo con
         * cualquier payload que contuviera esas dos letras. */
        if (strcmp(resp, "OK") == 0)
            return SIM_AT_RESPONSE_COMMAND_OK;

        ESP_LOGD(TAG, "read_resp_values('%s'): linea intercalada descartada: '%s'",
                 key_word, resp); // [LOG-NOISE] idem: se llama en loop
    }

    return SIM_AT_RESPONSE_ERR_COMMAND_INVALID;
}

simcom_responses_err_t simcom_resp_read_ok(char* resp)
{
    for (int i = 0; i < SIM_AT_MAX_LINES; i++)
    {
        resp[0] = '\0';

        if (!simcom_get_resp(resp))
        {
            /* Este si queda en WARN: resp_read_ok se llama una vez por comando, no en
             * loop, asi que un buffer vacio aca es una anomalia real que hay que ver. */
            ESP_LOGW(TAG, "resp_read_ok: buffer vacio (ningun OK/ERROR recibido)");
            return SIM_AT_RESPONSE_ERR_COMMAND_INVALID;
        }

        if (strcmp(resp, "OK") == 0)
            return SIM_AT_RESPONSE_COMMAND_OK;
        if (strstr(resp, "ERROR") != NULL)
            return SIM_AT_RESPONSE_ERR_COMMAND_ERROR;

        ESP_LOGW(TAG, "resp_read_ok: linea intercalada descartada: '%s'", resp);
    }

    return SIM_AT_RESPONSE_ERR_COMMAND_INVALID;
}

bool simcom_was_reset(void)
{
    return g_modem_reset;
}

void simcom_clear_reset(void)
{
    g_modem_reset = false;
}

BaseType_t simcom_parser_task_create(void)
{
    BaseType_t ret = xTaskCreate(
        _s_parser_task_fn, "sim_at_parser",
        SIM_AT_PARSER_TASK_STACK, NULL,
        SIM_AT_PARSER_TASK_PRIO, &s_parser_task);
    return ret;
}

void simcom_parser_task_delete(void)
{
    if (s_parser_task)
    {
        vTaskDelete(s_parser_task);
        s_parser_task = NULL;
    }
}

/* End of file */