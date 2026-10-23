#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "basic_at";

simcom_err_t simcom_wait_modem_ready(void)
{
    int retries = 10;

    while (retries--)
    {
        if (simcom_cmd_sync("AT\r\n", 1000) == SIM_AT_OK)
        {
            ESP_LOGI(TAG, "SIMCom ready!");
            return SIM_AT_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGE(TAG, "SIMCom not initialized correctly");
    return SIM_AT_ERR_RESPONSE;
}


simcom_err_t simcom_wait_atready(void)
{
    simcom_err_t err;
    err = simcom_wait_resp(10000);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "SIMCom not initilized");
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "*ATREADY", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with *ATREADY response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    int err_code;
    if (sscanf(data, "%d", &err_code) != 1)
    return SIM_AT_ERR_RESPONSE;

    if (err_code != 1)
    {
        ESP_LOGE(TAG, "SIMCom not initilized");
        return err;
    }

    return SIM_AT_OK;
}

simcom_err_t simcom_comm_test(void)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT\r\n", 5000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error sending AT command: %s", simcom_err_to_str(err));
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

simcom_err_t simcom_enable_echo(bool enable)
{
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "ATE%d\r\n", enable);

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