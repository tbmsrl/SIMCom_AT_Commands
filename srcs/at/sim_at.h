#ifndef SIM_AT_H
#define SIM_AT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "simcom_types.h"
#include "simcom_config.h"

/**
 * -------------------------------------
 * ----- [ Compile-time tunables ] -----
 * -------------------------------------
 */

// max single AT command length (including CR/LF)
#ifndef SIM_AT_MAX_CMD_LEN
#define SIM_AT_MAX_CMD_LEN        256U 
#endif

// default max response length the library will accept per command
#ifndef SIM_AT_MAX_RESP_LEN
#define SIM_AT_MAX_RESP_LEN       1024U   
#endif

// number of in-flight commands supported without dynamic alloc
#ifndef SIM_AT_MAX_PENDING_COMMANDS
#define SIM_AT_MAX_PENDING_COMMANDS 4U    
#endif

#define UART_MAX_WAITTIME 50 // [ms]

// TODO: Capaz conviene utilizar un extern para estos 2
void simcom_set_config(simcom_config_t* config);
void simcom_set_init_flag(bool init_f);

simcom_err_t simcom_sem_create(void);
void simcom_sem_delete(void);

BaseType_t simcom_parser_task_create(void);
void simcom_parser_task_delete(void);

/**
 * -----------------------------------------
 * ----- [ Response / callback types ] -----
 * ----------------------------------------- 
 */

/**
 * ------------------------------------------
 * ----- [ Core API: issuing commands ] -----
 * ------------------------------------------ 
 */

/**
 * @brief Send an AT command synchronously (blocking - do not call from ISR).
 *
 * @param cmd NUL-terminated AT command (e.g. "AT+CGSN\r\n"). Must be <= SIM_AT_MAX_CMD_LEN.
 * @param timeout_ms how long to wait for final response (OK/ERROR). If zero, uses default configured timeout.
 *
 * @return 
 *  - SIM_AT_OK on success
 *  - SIM_AT_ERR_NOT_INIT
 *  - SIMCOM_ERR_TIMEOUT if the command timed out. Note: if simcom_was_reset() returns
 *    true after a timeout, the modem reset during the wait — treat as SIMCOM_ERR_MODEM_RESET.
 *  - SIM_AT_ERR_UART
 *
 */
simcom_err_t simcom_cmd_sync(const char *cmd, uint32_t timeout_ms);

/**
 * @brief Waits for AT command response (blocking - do not call from ISR).
 *
 * [ASYNC-FIX] Cubre dos casos distintos que conviven en la libreria:
 *
 *   a) Un resultado diferido con prefijo propio (+CNTP:, +CMQTTPUB:, ...), que llega
 *      despues del OK inicial. Se detecta buscandolo en el ring buffer.
 *
 *   b) Un "OK" suelto que no corresponde a ningun cmd_sync (por ejemplo el que llega
 *      despues de escribir el dato crudo tras un prompt '>'). Se detecta por semaforo.
 *
 * Cuando se sabe QUE linea se espera conviene usar simcom_wait_resp_for(), que es
 * inmune a que haya quedado una linea diferida vieja dando vueltas en el buffer.
 * 
 * @param timeout_ms how long to wait for final response (OK/ERROR). If zero, uses default configured timeout.
 *                   Valores mayores a 60 s se acotan a 60 s (hay callers historicos
 *                   que pasaban portMAX_DELAY, lo que dejaba la radio prendida sin tope).
 * 
 * @return
 *  - SIM_AT_OK on success
 *  - SIM_AT_ERR_NOT_INIT
 *  - SIMCOM_ERR_TIMEOUT
 *  - SIM_AT_ERR_UART
 */
simcom_err_t simcom_wait_resp(uint32_t timeout_ms);

/**
 * [ASYNC-FIX] @brief Espera una linea de respuesta concreta, por prefijo.
 *
 * Version estricta de simcom_wait_resp(). Busca UNA linea puntual en el ring buffer
 * (ej: "+CNTP:", "+CMQTTPUB:"), asi que no se puede confundir ni con la senal de otro
 * comando ni con una linea diferida distinta que haya quedado dando vueltas.
 *
 * Usarla siempre que se sepa que linea se espera. simcom_wait_resp() queda para el caso
 * del "OK" suelto posterior al prompt '>', donde no hay prefijo que buscar.
 *
 * NO consume la linea: leerla despues con simcom_read_resp_values().
 *
 * @param prefix     Prefijo esperado, con los dos puntos incluidos (ej: "+CMQTTPUB:").
 * @param timeout_ms Tope de espera. 0 usa el default. Valores mayores a 60 s se acotan.
 *
 * @return
 *  - SIM_AT_OK si la linea aparecio
 *  - SIMCOM_ERR_TIMEOUT si no aparecio a tiempo
 *  - SIM_AT_ERR_NOT_INIT / SIM_AT_ERR_INVALID_ARG
 */
simcom_err_t simcom_wait_resp_for(const char *prefix, uint32_t timeout_ms);

/**
 * ---------------------------------------------
 * ----- [ Core API: requesting commands ] -----
 * --------------------------------------------- 
 */

/**
 * @brief Get next response from ring buffer
 * 
 * @param buf Response buffer
 * 
 * @return False is there is no new responses, True otherwise
 */
bool simcom_get_resp(char* buf);

/**
 * @brief Ignore next response from ring buffer
 */
void simcom_ignore_resp(void);

/**
 * @brief Verify the response and get the index of the values
 *
 * [URC-FIX] Saltea lineas que no corresponden (URCs intercalados) hasta encontrar la
 * que tiene la palabra clave, un OK o un ERROR. Antes leia UNA sola linea y decidia con
 * eso, asi que un URC colado entre el comando y su respuesta invalidaba el comando.
 * 
 * @param resp Response buffer
 * @param key_word Word to verify if present in the response
 * @param start_response Start index to response values
 * 
 * @returns
 *  - SIM_AT_OK if succeded
 *  - SIM_AT_COMMAND_OK an OK was received
 *  - SIM_AT_ERR_COMMAND_ERROR an ERROR was received
 *  - SIM_AT_ERR_COMMAND_INVALID invalid response
 *  - SIM_AT_ERR_INVALID_FORMAT invalid response format
 */
simcom_responses_err_t simcom_read_resp_values(char* resp, const char* key_word, char** index);

/**
 * @brief Verify is the response is OK
 *
 * [URC-FIX] Idem: saltea lineas intercaladas hasta dar con el OK o el ERROR.
 * 
 * @param resp Response buffer
 * 
 * @returns
 *  - SIM_AT_COMMAND_OK an OK was received
 *  - SIM_AT_ERR_COMMAND_ERROR an ERROR was received
 *  - SIM_AT_ERR_COMMAND_INVALID invalid response
 */
simcom_responses_err_t simcom_resp_read_ok(char* resp);

/**
 * -----------------------------------------
 * ----- [ Modem reset detection API ] -----
 * -----------------------------------------
 */

/**
 * @brief Returns true if a modem reset (*ATREADY: 1) was detected since the
 *        last call to simcom_clear_reset().
 *
 * Intended usage in the main task error path:
 * @code
 *   err = simcom_cmd_sync("AT+CREG?\r\n", 9000);
 *   if (err != SIM_AT_OK) {
 *       if (simcom_was_reset()) {
 *           simcom_clear_reset();
 *           // run full re-init sequence, then retry
 *       }
 *   }
 * @endcode
 *
 * @note This flag is set by the parser task and read by the main task.
 *       The flag is declared volatile; no additional locking is needed for
 *       a single-reader / single-writer scenario.
 *
 * @return true if a modem reset was detected, false otherwise
 */
bool simcom_was_reset(void);

/**
 * @brief Clears the modem reset flag.
 *
 * Must be called by the main task after it has handled the reset event and
 * completed the re-initialization sequence (including ATE0).
 */
void simcom_clear_reset(void);

/**
 * -----------------------
 * Utility helpers
 * ----------------------- 
 */

/**
 * @brief Flush UART RX buffer inside library context.
 */
simcom_err_t simcom_uart_flush_rx(void);

/**
 * -----------------------------------
 * ----- [ Diagnostics / debug ] -----
 * ----------------------------------- 
 */

/**
 * @brief Enable library internal debug logging over ESP_LOG (calls to ESP_LOGI/W).
 * 
 * @param en bool - Enable/Disable sim at debug
 * 
 * @return SIM_AT_OK
 */
simcom_err_t simcom_enable_debug(bool en);

/**
 * @brief Returns the corresponding description for a sim error
 * 
 * @param err Error code
 * 
 * @return Error string
 */
const char* simcom_err_to_str(simcom_err_t err);

/**
 * @brief Returns the corresponding description for a sim response error
 * 
 * @param err Error code
 * 
 * @return Error string
 */
const char* simcom_resp_err_to_str(simcom_responses_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* SIM_AT_H */