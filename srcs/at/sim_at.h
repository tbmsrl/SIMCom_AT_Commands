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

 // TODO: Revisar descripción
 // TODO: Capaz todo esto no sirve porque es para lo de async que medio medio
/**
 * Response object returned to user callbacks or filled by sync call.
 * The library will not malloc; the caller must provide buffers to receive
 * response data in sync calls. Async callbacks receive a pointer valid only
 * for the duration of the callback (unless user copies contents).
 */
// typedef struct {
//     const char *cmd;    // pointer to the command string that originated this response (const view)
//     const char *raw;    // pointer to raw response buffer (NUL-terminated). Valid only during callback lifetime.
//     size_t raw_len;     // length of raw response (bytes, excluding NUL)
//     int final_code;     // 0 = OK, non-zero = ERROR or module-specific numeric error if parsed
//     void *user_ctx;     // user-provided context pointer
// } sim_at_response_t;

// /**
//  * Async completion callback signature.
//  * Runs in library context (not in caller task). Keep short — do not block.
//  */
// typedef void (*sim_at_cmd_cb_t)(const sim_at_response_t *resp);

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
 *  - SIM_AT_ERR_NOT_INIT.
 *  - SIMCOM_ERR_TIMEOUT
 *  - SIM_AT_ERR_UART
 *
 */
simcom_err_t simcom_cmd_sync(const char *cmd, uint32_t timeout_ms);

/**
 * @brief Waits for AT command response (blocking - do not call from ISR).
 * 
 * @param timeout_ms how long to wait for final response (OK/ERROR). If zero, uses default configured timeout.
 * 
 * @return
 *  - SIM_AT_OK on success
 *  - SIM_AT_ERR_NOT_INIT.
 *  - SIMCOM_ERR_TIMEOUT
 *  - SIM_AT_ERR_UART
 */
simcom_err_t simcom_wait_resp(uint32_t timeout_ms);

/**
 * Send an AT command asynchronously.
 *
 * Parameters:
 * - cmd: NUL-terminated AT command; the library will copy the command into an internal static slot (bounded by SIM_AT_MAX_CMD_LEN).
 * - cb: optional callback invoked on completion. If NULL, command is fire-and-forget.
 * - user_ctx: user pointer forwarded into response.user_ctx.
 * - timeout_ms: per-command timeout (0 = default).
 *
 * Note: to avoid dynamic allocation, only SIM_AT_MAX_PENDING_COMMANDS may be queued/in-flight.
 * If queue is full, function returns SIM_AT_ERR_BUSY.
 *
 * The callback executes in the library context; keep it short. The sim_at_response_t::raw pointer passed to cb is only valid during the callback.
 */
// simcom_err_t sim_at_cmd_async(const char *cmd, sim_at_cmd_cb_t cb, void *user_ctx, uint32_t timeout_ms);

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
 * @param resp Response buffer
 * 
 * @returns
 *  - SIM_AT_COMMAND_OK an OK was received
 *  - SIM_AT_ERR_COMMAND_ERROR an ERROR was received
 *  - SIM_AT_ERR_COMMAND_INVALID invalid response
 */
simcom_responses_err_t simcom_resp_read_ok(char* resp);

/**
 * -----------------------
 * Utility helpers
 * ----------------------- 
 */

/**
 * @brief Flush UART RX buffer inside library context.
 */
simcom_err_t simcom_uart_flush_rx(void);

// TODO: Completar
/**
 * Lower-level control: toggle DTR/PWRKEY/RST lines if configured.
 * - state: true = asserted (active), false = deasserted.
 * Library will validate configured pins; if pin == -1 returns SIM_AT_ERR_INVALID_ARG.
 * These functions are provided to allow app-managed power sequences.
 */
// simcom_err_t sim_at_control_dtr(bool state);
// simcom_err_t sim_at_control_pwrkey(bool state);
// simcom_err_t sim_at_control_reset(bool state);

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
