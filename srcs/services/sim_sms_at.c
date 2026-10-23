#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "sms_at";


simcom_err_t simcom_sms_new_indications_set(uint8_t mode, uint8_t mt, uint8_t bm, uint8_t ds, uint8_t bfr)
{
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CNMI=%d,%d,%d,%d,%d\r\n", mode, mt, bm, ds, bfr);

    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CNMI commands: %s", simcom_err_to_str(err));
        return err;
    }

    // Read OK responss
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } else 
    {
        ESP_LOGI(TAG, "SMS notifications successfuly deactivated!");
    }
    
    return SIM_AT_OK;

}