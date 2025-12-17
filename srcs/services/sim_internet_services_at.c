#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "internet_services_at";

simcom_err_t sim_at_get_ntp_config(void)
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

    // Just logs the current NTP config

    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 

    return SIM_AT_OK;
}

simcom_err_t sim_at_set_ntp_config(const char* host, int timezone)
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

simcom_err_t sim_at_ntp_update_system_time(sim_at_ntp_err_code_t* ntp_err)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CNTP\r\n", 9000);
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

    // Wait for err code
    // TODO: Revisar si existe una forma mejor de hacerlo
    // El problema es que si no va a mandar la respuesta en cualquier momento, y no sé si se puede seguir 
    // interactuando mientras
    // Sino ver de encontrar cuál es el timeout configurado para el NTP
    err = simcom_wait_resp(portMAX_DELAY); 
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
        return SIM_AT_ERR_RESPONSE;
    *ntp_err = err_code;

    return SIM_AT_OK;
}