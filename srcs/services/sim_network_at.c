#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "network_at";


//TOMI DEBUG: Gets the current operator in MCC/MNC format
simcom_err_t simcom_get_operator(simcom_operator_info_t *op)
{
    simcom_err_t err = simcom_cmd_sync("AT+CPSI?\r\n", 9000);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CPSI? command: %s", simcom_err_to_str(err));
        return err;
    }

    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;

    simcom_responses_err_t resp_err =
        simcom_read_resp_values(resp, "+CPSI", &data);

    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error parsing AT+CPSI? response: %s",
                 simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    // Reset
    op->mode = 0;
    op->format = 0;
    op->act = 0;
    op->operator_name[0] = '\0';

    // CPSI format example:
    // +CPSI: LTE,Online,722-34,...
    char tech[32] = {0};
    char status[32] = {0};
    char plmn[16] = {0};

    int ret = sscanf(data,
                     "%31[^,],%31[^,],%15[^,]",
                     tech,
                     status,
                     plmn);

    if (ret < 3)
    {
        ESP_LOGE(TAG, "Failed parsing CPSI data: %s", data);
        return SIM_AT_ERR_RESPONSE;
    }

    // Guardamos PLMN crudo
    strncpy(op->operator_name, plmn, sizeof(op->operator_name) - 1);

    // Parse MCC/MNC (722-34 → Argentina Personal)
    int mcc = 0, mnc = 0;

    if (sscanf(plmn, "%d-%d", &mcc, &mnc) == 2)
    {
        op->mcc = mcc;
        op->mnc = mnc;
    }
    else
    {
        ESP_LOGW(TAG, "Could not parse MCC/MNC: %s", plmn);
    }

    // opcional: guardar tecnología como "act"
    if (strstr(tech, "LTE")) op->act = 4;
    else if (strstr(tech, "WCDMA")) op->act = 3;
    else if (strstr(tech, "GSM")) op->act = 2;

    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "OK not received for AT+CPSI?");
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK;
}

//Configura el modo de selección del operador: (manual/automático)
simcom_err_t simcom_set_operator_selection(sim_cops_mode_t mode)
{
    //Build command
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+COPS=%d\r\n", mode);

    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 180000); // 180s: spec allows up to 3min for network search
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error sending AT+COPS=%d command: %s", mode, simcom_err_to_str(err));
        return err;
    }

    // AT+COPS= only returns OK, no "+COPS:" prefix to parse
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK;
}


simcom_err_t simcom_net_reg(sim_network_registration_stat_t *stat)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CREG?\r\n", 9000);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CREG? commands: %s", simcom_err_to_str(err));
        return err;
    }

    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CREG", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CREG? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    // Get network status
    // TODO: N no se utiliza
    int pN, pStat;
    if (sscanf(data, "%d,%d", &pN, &pStat) != 2)
        return SIM_AT_ERR_RESPONSE;

    *stat = pStat;

    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK;
}


const char *simcom_net_stat_to_str(sim_network_registration_stat_t stat)
{
    switch (stat)
    {
    case NOT_REGISTERED: return "not registered, ME is not currently searching a new operator to register to";
    case REGISTERED: return "registered, home network";
    case SEARCHING: return "not registered, but ME is currently searching a new operator to register to";
    case REGISTRATION_DENIED: return "registration denied";
    case UNKNOWN: return "unknown";
    case ROAMING: return "registered, roaming";
    case SMS_ONLY: return "registered for \"SMS only\", home network";
    case SMS_ONLY_ROAMING: return "registered for \"SMS only\", roaming";
    case EMERGENCY: return "attached for emergency bearer services only";
    default: return "unknown";
    }
}