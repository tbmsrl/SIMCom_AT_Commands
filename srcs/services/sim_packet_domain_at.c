#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "packet_domain_at";

simcom_err_t simcom_eps_net_reg(sim_eps_network_registration_stat_t* stat)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CEREG?\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CEREG command: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CEREG", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CEREG? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }
    
    // Parse two integers separated by a comma
    // TODO: Por el momento no se utiliza para nada, pero se guarda por las dudas
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

const char* simcom_sim_eps_net_stat_to_str(sim_eps_network_registration_stat_t stat)
{
    switch(stat)
    {
        case EPS_NOT_REGISTERED: return "not registered, ME is not currently searching an operator to register to";
        case EPS_REGISTERED: return "registered, home network";
        case EPS_SEARCHING: return "not registered, but ME is currently trying to attach or searching an operator to register to";
        case EPS_REGISTRATION_DENIED: return "";
        case EPS_UNKNOWN: return "registration denied";
        case EPS_ROAMING: return "unknown";
        case EPS_SMS_ONLY: return "registered, roaming";
        case EPS_SMS_ONLY_ROAMING: return "registered for \"SMS only\", home network";
        case EPS_EMERGENCY: return "attached for emergency bearer services only";\
        default: return "unknown";
    }
}

simcom_err_t simcom_get_packet_domain_attach(int* state)
{
    // Send command    
    simcom_err_t err = simcom_cmd_sync("AT+CGATT?\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGATT? command: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CGATT", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CGATT? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    // Parse integer
    if (sscanf(data, "%d", state) != 1)
        return SIM_AT_ERR_RESPONSE;
    
    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK; 
}

simcom_err_t simcom_set_packet_domain_attach(int state)
{
    if (state != 0 && state != 1)
        return SIM_AT_ERR_INVALID_ARG;

    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CGATT=%d\r\n", state); 

    // Send command    
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGATT=%d command: %s", state, simcom_err_to_str(err));
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

// TODO: El problema si tiene muchos cid es como los devuelve, porque acá nomás devuelve el primero.
// Habría que utilizar una array, o elegir que cid queremos verificar
simcom_err_t simcom_get_pdp_context_activate(int* cid, int* state)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CGACT?\r\n", 2000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGACT command: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CGACT", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CGACT? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }
    
    // Parse two integers separated by a comma
    if (sscanf(data, "%d,%d", cid, state) != 2)
        return SIM_AT_ERR_RESPONSE;
    
    // TODO: En caso que haya muchos contextos de PDP podría haber problemas al leer las respuestas
    // Ver bien como hacer eso
    // Capaz controlar hasta que se reciba un OK
    
    // Reads OK
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK; 
}

simcom_err_t simcom_set_pdp_context_activate(int cid, int state)
{
    if (state != 0 && state != 1)
        return SIM_AT_ERR_INVALID_ARG;
    if (cid < 1 || cid > 15)
        return SIM_AT_ERR_INVALID_ARG;

    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CGACT=%d,%d\r\n", state, cid);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGACT=%d,%d command: %s", state, cid, simcom_err_to_str(err));
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

simcom_err_t simcom_get_pdp_context(void)
{
    // Sends command
    simcom_err_t err = simcom_cmd_sync("AT+CGDCONT?\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGDCONT? command: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CGDCONT", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CGDCONT? response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    // Parse values
    // TODO: Ver si es necesario o útil

    // TODO: En caso que haya muchos contextos de PDP podría haber problemas al leer las respuestas
    // Ver bien como hacer eso
    // Capaz controlar hasta que se reciba un OK

    // Reads OK
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK; 
}

const char* simcom_pdp_type_to_str(sim_pdp_type_t pdp_type)
{
    switch(pdp_type)
    {
        case PDP_IP: return "PDP_IP";
        case PDP_IPV6: return "PDP_IPV6"; 
        case PDP_IPV4V6: return "PDP_IPV4V6";
        default: return NULL;
    }
}

simcom_err_t simcom_set_pdp_context(int cid, sim_pdp_type_t pdp_type, const char* apn)
{
    if (cid < 1 || cid > 15)
        return SIM_AT_ERR_INVALID_ARG;
    if (apn == NULL)
        return SIM_AT_ERR_INVALID_ARG;

    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CGDCONT=%d,\"%s\",\"%s\"\r\n", cid, simcom_pdp_type_to_str(pdp_type), apn);
    
    // Sends command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGDCONT command: %s", simcom_err_to_str(err));
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

simcom_err_t simcom_show_pdp_addr(int* cid, char* addr)
{
    // Sends command
    simcom_err_t err = simcom_cmd_sync("AT+CGPADDR\r\n", 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CGPADDR command: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Reads response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CGPADDR", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CGPADDR response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }
    
    // Parse two integers separated by a comma
    if (sscanf(data, "%d,%s", cid, addr) != 2)
        return SIM_AT_ERR_RESPONSE;
    
    // TODO: En caso que haya muchos contextos de PDP podría haber problemas al leer las respuestas
    // Ver bien como hacer eso
    // Capaz controlar hasta que se reciba un OK

    // Read OK responss
    resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK; 
}

simcom_err_t simcom_ping(const char* dest_addr)
{
    // Command
    // Always works with IPv4, altough it could be configured
    // Use default parameters
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CPING=\"%s\",1\r\n", dest_addr);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CPING command: %s", simcom_err_to_str(err));
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

    // TODO: Ignora los resultados del ping, solamente verifica el Ok.

    return SIM_AT_OK; 
}