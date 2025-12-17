#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "mqtt_at";

const char* simcom_mqtt_err_to_str(sim_mqtt_err_codes_t err)
{
    switch (err)
    {
        case SIM_MQTT_OK:                                  return "Operation succeeded";
        case SIM_MQTT_ERR_FAIL:                            return "General failure";
        case SIM_MQTT_ERR_BAD_UTF8_STRING:                 return "Bad UTF-8 string";
        case SIM_MQTT_ERR_SOCK_CONNECT_FAIL:               return "Socket connect failed";
        case SIM_MQTT_ERR_SOCK_CREATE_FAIL:                return "Socket creation failed";
        case SIM_MQTT_ERR_SOCK_CLOSE_FAIL:                 return "Socket close failed";
        case SIM_MQTT_ERR_MSG_RECEIVE_FAIL:                return "Message receive failed";
        case SIM_MQTT_ERR_NETWORK_OPEN_FAIL:               return "Network open failed";
        case SIM_MQTT_ERR_NETWORK_CLOSE_FAIL:              return "Network close failed";
        case SIM_MQTT_ERR_NETWORK_NOT_OPENED:              return "Network not opened";
        case SIM_MQTT_ERR_CLIENT_INDEX:                    return "Invalid client index";
        case SIM_MQTT_ERR_NO_CONNECTION:                   return "No active connection";
        case SIM_MQTT_ERR_INVALID_PARAMETER:               return "Invalid parameter";
        case SIM_MQTT_ERR_NOT_SUPPORTED:                   return "Operation not supported";
        case SIM_MQTT_ERR_CLIENT_BUSY:                     return "Client is busy";
        case SIM_MQTT_ERR_REQUIRE_CONN_FAIL:               return "Require connection failed";
        case SIM_MQTT_ERR_SOCK_SEND_FAIL:                  return "Socket sending failed";
        case SIM_MQTT_ERR_TIMEOUT:                         return "Timeout occurred";
        case SIM_MQTT_ERR_TOPIC_EMPTY:                     return "Topic is empty";
        case SIM_MQTT_ERR_CLIENT_IN_USE:                   return "Client is already used";
        case SIM_MQTT_ERR_CLIENT_NOT_ACQUIRED:             return "Client not acquired";
        case SIM_MQTT_ERR_CLIENT_NOT_RELEASED:             return "Client not released";
        case SIM_MQTT_ERR_LEN_OUT_OF_RANGE:                return "Length out of range";
        case SIM_MQTT_ERR_NETWORK_OPENED:                  return "Network already opened";
        case SIM_MQTT_ERR_PACKET_FAIL:                     return "Packet failure";
        case SIM_MQTT_ERR_DNS:                             return "DNS error";
        case SIM_MQTT_ERR_SOCK_CLOSED_BY_SERVER:           return "Socket closed by server";
        case SIM_MQTT_ERR_CONN_REFUSED_BAD_PROTOCOL:       return "Connection refused: unaccepted protocol version";
        case SIM_MQTT_ERR_CONN_REFUSED_ID_REJECTED:        return "Connection refused: identifier rejected";
        case SIM_MQTT_ERR_CONN_REFUSED_SERVER_UNAVAILABLE: return "Connection refused: server unavailable";
        case SIM_MQTT_ERR_CONN_REFUSED_BAD_CREDENTIALS:    return "Connection refused: bad username or password";
        case SIM_MQTT_ERR_CONN_REFUSED_NOT_AUTHORIZED:     return "Connection refused: not authorized";
        case SIM_MQTT_ERR_HANDSHAKE_FAIL:                  return "Handshake failed";
        case SIM_MQTT_ERR_CERT_NOT_SET:                    return "Certificate not set";
        case SIM_MQTT_ERR_OPEN_SESSION_FAIL:               return "Open session failed";
        case SIM_MQTT_ERR_DISCONNECT_FAIL:                 return "Disconnect from server failed";
        default:                                           return "Unknown MQTT error";
    }
}

simcom_err_t simcom_mqtt_service_start(void)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CMQTTSTART\r\n", 12000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTSTART commands: %s", simcom_err_to_str(err));
        return err;
    }

    // TODO: Si devuelve ERROR es que ya se encuentra inicializado

    // Read OK responss
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 
    
    // Parse response
    char *data;
    resp_err = simcom_read_resp_values(resp, "+CMQTTSTART", &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CNTP response: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }  
    
    int err_code;
    if (sscanf(data, "%d", &err_code) != 1)
        return SIM_AT_ERR_RESPONSE;


    if (err_code != SIM_MQTT_OK)
    {
        ESP_LOGE(TAG, "Error starting MQTT service: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_INTERNAL;
    }

    return SIM_AT_OK;
}

simcom_err_t simcom_mqtt_service_stop(void)
{
    // Send command
    simcom_err_t err = simcom_cmd_sync("AT+CMQTTSTOP\r\n", 12000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTSTOP commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTSTOP", &data);
    
    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
        return SIM_AT_OK;
    
    if (resp_err == SIM_AT_RESPONSE_ERR_COMMAND_ERROR)
    {
        ESP_LOGW(TAG, "MQTT service already stopped");
        return SIM_AT_OK;
    }
    
    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int err_code;
        if (sscanf(data, "%d", &err_code) != 1)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error with stopping MQTT service: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_ERR_RESPONSE;
}

simcom_err_t simcom_mqtt_client_acquire(int client_index, const char* client_id)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    if (client_id == NULL || strlen(client_id) > 128)
        return SIM_AT_ERR_INVALID_ARG;
    
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTACCQ=%d,\"%s\"\r\n", client_index, client_id);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTACCQ commands: %s", simcom_err_to_str(err));
        return err;
    }
        
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTACCQ", &data);

    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
        return SIM_AT_OK;
    
    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int aux, err_code;
        if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error with acquiring MQTT client: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_ERR_RESPONSE;
}

simcom_err_t simcom_mqtt_client_release(int client_index)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;  
    
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTREL=%d\r\n", client_index);
    
    // Send command    
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTREL commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTREL", &data);

    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
        return SIM_AT_OK;
    
    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int aux, err_code;
        if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error with acquiring MQTT client: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK;
}

simcom_err_t simcom_mqtt_server_connect(int client_index, const char* server_addr, int keepalive_time, int clean_session)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    int server_addr_len = strlen(server_addr);
    if (server_addr_len < 9 || server_addr_len > 256)  
        return SIM_AT_ERR_INVALID_ARG;
    if (keepalive_time < 1 || keepalive_time > 64800)
        return SIM_AT_ERR_INVALID_ARG;
    if (clean_session != 0 && clean_session != 1)
        return SIM_AT_ERR_INVALID_ARG;
    
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTCONNECT=%d,\"%s\",%d,%d\r\n", client_index, server_addr, keepalive_time, clean_session);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTCONNECT commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTCONNECT", &data);   
    
    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTCONNECT", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            
            if (err_code != SIM_MQTT_OK)
            {
                ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
                return SIM_AT_ERR_RESPONSE;
            }
            return SIM_AT_OK;   
        }
    }

    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int aux, err_code;
        if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    if (resp_err == SIM_AT_RESPONSE_ERR_COMMAND_ERROR)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTCONNECT", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
            return SIM_AT_ERR_RESPONSE;
        }
    }
    
    return SIM_AT_ERR_RESPONSE;
}

simcom_err_t simcom_mqtt_server_disconnect(int client_index, int timeout)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    if (timeout < 0 || timeout > 180)
        return SIM_AT_ERR_INVALID_ARG;
    
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTDISC=%d,%d\r\n", client_index, timeout);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTDISC commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTDISC", &data);   
    
    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTDISC", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            
            if (err_code != SIM_MQTT_OK)
            {
                ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
                return SIM_AT_ERR_RESPONSE;
            }
            return SIM_AT_OK;   
        }
    }

    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int aux, err_code;
        if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    if (resp_err == SIM_AT_RESPONSE_ERR_COMMAND_ERROR)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTDISC", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
            return SIM_AT_ERR_RESPONSE;
        }
    }
    
    return SIM_AT_ERR_RESPONSE;
}

simcom_err_t simcom_mqtt_topic_set(int client_index, const char* topic)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    int topic_len = strlen(topic);
    if (topic == NULL || topic_len > 1024)
        return SIM_AT_ERR_INVALID_ARG;
    
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTTOPIC=%d,%d\r\n", client_index, (int)strlen(topic));
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 2000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTTOPIC commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // No se analiza el error en caso que falle

    // Wait for input response
    char resp[SIM_AT_MAX_RESP_LEN];    
    simcom_get_resp(resp);
    if (strstr(resp, ">") == NULL)
       return SIM_AT_ERR_RESPONSE; 
    
    // Send topic
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "%s\r\n", topic);
    err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error sending topic: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Read OK response
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 
    
    return SIM_AT_OK;
}

simcom_err_t simcom_mqtt_payload_set(int client_index, const char* payload)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    int payload_len = strlen(payload);
    if (payload == NULL || payload_len > 10240)
        return SIM_AT_ERR_INVALID_ARG;

    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTPAYLOAD=%d,%d\r\n", client_index, (int)strlen(payload));
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 2000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTPAYLOAD commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Wait for input respose
    char resp[SIM_AT_MAX_RESP_LEN];    
    simcom_get_resp(resp);
    if (strstr(resp, ">") == NULL)
       return SIM_AT_ERR_RESPONSE; // TODO: Poner otro, o analizar el error después
    
    // Send payload
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "%s\r\n", payload);
    err = simcom_cmd_sync(cmd, 2000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error sending payload: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Read OK response
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 
    
    return SIM_AT_OK;  
}

simcom_err_t simcom_mqtt_publish(int client_index, int qos, int pub_timeout)
{
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    if (qos < 0 || qos > 2)
        return SIM_AT_ERR_INVALID_ARG;
    if (pub_timeout < 1 || pub_timeout > 180)
        return SIM_AT_ERR_INVALID_ARG;
        
    // Command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTPUB=%d,%d,%d\r\n", client_index, qos, pub_timeout);
    
    // Send command
    simcom_err_t err = simcom_cmd_sync(cmd, pub_timeout*1000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTPUB commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    // Parse response
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data;
    simcom_responses_err_t resp_err = simcom_read_resp_values(resp, "+CMQTTPUB", &data);   
    
    if (resp_err == SIM_AT_RESPONSE_COMMAND_OK)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTPUB", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            
            if (err_code != SIM_MQTT_OK)
            {
                ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
                return SIM_AT_ERR_RESPONSE;
            }
            return SIM_AT_OK;   
        }
    }

    if (resp_err == SIM_AT_RESPONSE_OK)
    {
        int aux, err_code;
        if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
            return SIM_AT_ERR_RESPONSE;
        ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    if (resp_err == SIM_AT_RESPONSE_ERR_COMMAND_ERROR)
    {
        resp_err = simcom_read_resp_values(resp, "+CMQTTPUB", &data);
        if (resp_err == SIM_AT_RESPONSE_OK)
        {
            int aux, err_code;
            if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
                return SIM_AT_ERR_RESPONSE;
            ESP_LOGE(TAG, "Error connecting to MQTT server: %s", simcom_mqtt_err_to_str(err_code));
            return SIM_AT_ERR_RESPONSE;
        }
    }
    
    return SIM_AT_ERR_RESPONSE;
}