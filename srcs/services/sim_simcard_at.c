#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "simcard_at";

simcom_err_t simcom_get_simcard_pin_info(sim_simcard_pin_code_t* code)
{
    // Sends command
    simcom_err_t err = simcom_cmd_sync("AT+CPIN?\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CPIN? commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CPIN", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CPIN? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    // Parse integer
    char code_str[16];
    if (sscanf(data, "%s", code_str) != 1)
        return SIM_AT_ERR_RESPONSE;
    
    // Parse SIM Card code
    if (strcmp(code_str, "READY") == 0)
        *code = SIMCARD_READY;
    else if (strcmp(code_str, "SIM PIN") == 0)
        *code = SIMCARD_SIM_PIN;
    else if (strcmp(code_str, "SIM PUK") == 0)
        *code = SIMCARD_SIM_PUK;
    else if (strcmp(code_str, "PH-SIM PIN") == 0)
        *code = SIMCARD_PH_SIM_PIN;
    else if (strcmp(code_str, "SIM PIN2") == 0)
        *code = SIMCARD_SIM_PIN_2;
    else if (strcmp(code_str, "SIM PUK2") == 0)
        *code = SIMCARD_SIM_PUK_2;
    else if (strcmp(code_str, "PH-NET PIN") == 0)
        *code = SIMCARD_PH_NET_PIN;
    else
        ESP_LOGE(TAG, "The SIM Card code was not recognized: %s", code_str);

    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }
    
    return SIM_AT_OK; 
}