#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "internet_services_at";

/* [NTP-FIX] Tope de espera del resultado diferido +CNTP: <err>.
 * Antes era portMAX_DELAY, o sea que un modulo que nunca contestara dejaba la tarea
 * colgada con la radio prendida hasta que saltara el watchdog. */
#define NTP_RESULT_TIMEOUT_MS   20000

simcom_err_t simcom_ntp_config_get(void)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CNTP?\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CNTP? commands: %s", simcom_err_to_str(err));
        return err;
    }

    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CNTP", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CNTP? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    ESP_LOGI(TAG, "Configuracion NTP actual: %s", data); // [NTP-FIX] antes no logueaba nada

    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 

    return SIM_AT_OK;
}

simcom_err_t simcom_ntp_config_set(const char* host, int timezone)
{
    // Es cada 15 min que considera
    int configTimezone = timezone * 4;

    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CNTP=\"%s\",%d\r\n", host, configTimezone);

    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CNTP commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Read OK responss
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 
    
    return SIM_AT_OK;
}

simcom_err_t simcom_ntp_sys_time_update(sim_at_ntp_err_code_t* ntp_err)
{
    /* [NTP-FIX] *ntp_err se invalida al entrar.
     *
     * Antes, TODOS los caminos de error volvian sin tocarlo, asi que el caller decidia
     * con lo que hubiera quedado en su stack. Si esa basura valia justo
     * NTP_OPERATION_SUCCEEDED, se daba el sync por bueno y se leia el RTC del modulo sin
     * sincronizar: el bug de la fecha de 2069 entrando por la puerta de atras. */
    if (ntp_err != NULL)
        *ntp_err = (sim_at_ntp_err_code_t)-1;

    char resp[SIM_AT_MAX_RESP_LEN];

    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CNTP\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CNTP commands: %s", simcom_err_to_str(err));
        return err;
    }

    // Read OK responss
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 

    /* [NTP-FIX] Espera acotada del resultado diferido.
     *
     * Antes: simcom_wait_resp(portMAX_DELAY). Dos problemas. Uno, sin tope: un modulo
     * mudo dejaba la tarea colgada con la radio prendida. Dos, wait_resp bloqueaba en el
     * semaforo, y con una cuenta colgada de un comando anterior volvia al instante sin
     * que hubiera llegado nada; entonces el parseo de abajo leia la linea equivocada y
     * reportaba "Error with AT+CNTP response: SIM_AT_RESPONSE_COMMAND_OK".
     *
     * Ahora wait_resp busca la linea "+CNTP:" por contenido en el ring buffer, asi que
     * no se puede confundir con la senal de otro comando. */
    err = simcom_wait_resp_for("+CNTP:", NTP_RESULT_TIMEOUT_MS);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error waiting for +CNTP response: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char *data;
    resp_err = simcom_read_resp_values(resp, "+CNTP", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CNTP response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 
    
    int err_code;
    if (sscanf(data, "%d", &err_code) != 1)
    {
        ESP_LOGE(TAG, "No se pudo parsear el codigo de +CNTP: '%s'", data); // [NTP-FIX]
        return SIM_AT_ERR_RESPONSE;
    }

    if (ntp_err != NULL)
        *ntp_err = (sim_at_ntp_err_code_t)err_code;

    return SIM_AT_OK;
}