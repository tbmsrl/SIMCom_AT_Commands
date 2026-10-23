#include "simcom.h"
#include "at/sim_at.h"

static const char *TAG = "mqtt_at";

/* [ASYNC-FIX] Tope de espera de la URC +CMQTTCONNECT, en ms. El loop viejo daba 5
 * vueltas de 200 ms (1 s efectivo) aunque el timeout del comando fuera de 9 s. */
#define MQTT_CONNECT_URC_TIMEOUT_MS   9000

/**
 * [URC-READ-FIX] Lee una URC diferida salteando el "OK" del comando.
 *
 * simcom_read_resp_values() devuelve COMMAND_OK apenas encuentra un "OK", aunque la
 * linea buscada venga DESPUES en el mismo buffer. Y es exactamente lo que pasa aca: el
 * modulo contesta "OK" y la URC junta, en el mismo chunk del UART:
 *
 *     <-- OK
 *     <-- +CMQTTCONNECT: 0,0
 *
 * Asi que hay que seguir leyendo mientras devuelva COMMAND_OK. Se sale ante la linea
 * buscada, ante un ERROR real, o cuando el buffer se vacia.
 *
 * NO se cambio la semantica de read_resp_values porque CMQTTACCQ y CMQTTREL dependen de
 * que COMMAND_OK signifique exito (en esos comandos el modulo contesta solo "OK" y la
 * linea "+CMQTTACCQ:" aparece unicamente ante error).
 */
static simcom_responses_err_t mqtt_read_urc(const char *key_word, char *resp, char **data)
{
    simcom_responses_err_t resp_err = SIM_AT_RESPONSE_ERR_COMMAND_INVALID;

    for (int i = 0; i < 8; i++)
    {
        resp_err = simcom_read_resp_values(resp, key_word, data);

        if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
            return resp_err;    // encontro la linea, un ERROR, o se quedo sin buffer
    }

    return resp_err;
}

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
    //Enviar comando
    simcom_err_t err = simcom_cmd_sync("AT+CMQTTSTART\r\n", 12000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTSTART commands: %s", simcom_err_to_str(err));
        return err;
    }

    //Leer OK (o ERROR si ya inició)
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_responses_err_t resp_err = simcom_resp_read_ok(resp);

    if (strstr(resp, "ERROR") != NULL)
    {
        ESP_LOGW(TAG, "MQTT ya se encuentra inicializado (ERROR response)");
        return SIM_AT_ERR_RESPONSE; 
    }
    
    if (resp_err != SIM_AT_RESPONSE_COMMAND_OK)
    {
        ESP_LOGE(TAG, "Ok response was not received: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    } 

    // -- Parse response --
    char *data = NULL;
    resp_err = simcom_read_resp_values(resp, "+CMQTTSTART", &data);
    
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "Error con la respuesta +CMQTTSTART: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }   
    
    //Extraer el código de error
    int err_code;
    if (sscanf(data, "%d", &err_code) != 1)
        return SIM_AT_ERR_RESPONSE;

    if (err_code != SIM_MQTT_OK)
    {
        ESP_LOGE(TAG, "Error starting MQTT service: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_INTERNAL;
    }

    //Si todo salió bien, devolver OK!
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
    //Sanity checks
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;

    int server_addr_len = strlen(server_addr);
    if (server_addr_len < 9 || server_addr_len > 256)
        return SIM_AT_ERR_INVALID_ARG;

    if (keepalive_time < 1 || keepalive_time > 64800)
        return SIM_AT_ERR_INVALID_ARG;

    if (clean_session != 0 && clean_session != 1)
        return SIM_AT_ERR_INVALID_ARG;

    //Build command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN,
             "AT+CMQTTCONNECT=%d,\"%s\",%d,%d\r\n",
             client_index, server_addr, keepalive_time, clean_session);

    //Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    ESP_LOGD(TAG, "[MQTT_CONNECT] cmd_sync ret: %d (%s)", err, simcom_err_to_str(err));

    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "AT+CMQTTCONNECT failed: %s", simcom_err_to_str(err));
        return err;
    }

    /* [ASYNC-FIX] Antes: loop de 5 x read_resp_values() con 200 ms de espera fija.
     * Dos problemas. Uno, cada vuelta en vacio consumia lo que hubiera en el buffer y
     * dejaba un log por iteracion. Dos, el tope real eran 5 vueltas, no un tiempo: si la
     * URC tardaba mas de ~1 s se perdia aunque el timeout del comando fuera de 9 s.
     *
     * wait_resp_for() espera la linea puntual por contenido, con granularidad de 20 ms y
     * un tope en tiempo de reloj. */
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data = NULL;

    err = simcom_wait_resp_for("+CMQTTCONNECT:", MQTT_CONNECT_URC_TIMEOUT_MS);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "No +CMQTTCONNECT URC received (%s)", simcom_err_to_str(err));
        return SIM_AT_ERR_RESPONSE;
    }

    simcom_responses_err_t resp_err = mqtt_read_urc("+CMQTTCONNECT", resp, &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "No se pudo leer +CMQTTCONNECT: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    //Parse response
    ESP_LOGD(TAG, "[MQTT_CONNECT] data: %s", data);

    int client_id = -1;
    int err_code = -1;

    if (sscanf(data, "%d,%d", &client_id, &err_code) != 2)
        return SIM_AT_ERR_RESPONSE;

    ESP_LOGD(TAG, "[MQTT_CONNECT] parsed: client=%d err=%d", client_id, err_code);

    //Success cases
    if (err_code == 0 || err_code == 19)
    {
        if (err_code == 19)
            ESP_LOGD(TAG, "Reusing existing MQTT connection");

        return SIM_AT_OK;
    }

    ESP_LOGE(TAG, "MQTT connect failed: %s",
             simcom_mqtt_err_to_str(err_code));

    return SIM_AT_ERR_RESPONSE;
}

simcom_err_t simcom_mqtt_server_disconnect(int client_index, int timeout)
{
    //Initial checks
    if (client_index != 0 && client_index != 1)
        return SIM_AT_ERR_INVALID_ARG;
    if (timeout < 0 || timeout > 180)
        return SIM_AT_ERR_INVALID_ARG;
    
    //Build command
    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTDISC=%d,%d\r\n", client_index, timeout);
    
    //Send command
    simcom_err_t err = simcom_cmd_sync(cmd, 9000);
    if (err != SIM_AT_OK)
    {   
        ESP_LOGE(TAG, "Error with AT+CMQTTDISC commands: %s", simcom_err_to_str(err));
        return err;
    }
    
    //Parse response
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
                ESP_LOGE(TAG, "Error disconnecting MQTT server: %s", simcom_mqtt_err_to_str(err_code));
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
        ESP_LOGE(TAG, "Error disconnecting MQTT server: %s", simcom_mqtt_err_to_str(err_code));
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
            ESP_LOGE(TAG, "Error disconnecting MQTT server: %s", simcom_mqtt_err_to_str(err_code));
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

    char cmd[SIM_AT_MAX_CMD_LEN];
    snprintf(cmd, SIM_AT_MAX_CMD_LEN, "AT+CMQTTPUB=%d,%d,%d\r\n", client_index, qos, pub_timeout);

    simcom_err_t err = simcom_cmd_sync(cmd, pub_timeout * 1000);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "Error with AT+CMQTTPUB commands: %s", simcom_err_to_str(err));
        return err;
    }

    /* [ASYNC-FIX] Esperar la URC real +CMQTTPUB, no asumir con el OK inicial.
     *
     * Antes: loop de (pub_timeout*1000)/200 vueltas, o sea 100 con pub_timeout=20. Cada
     * vuelta en vacio llamaba a read_resp_values y dejaba un log, con el modem prendido
     * esperando. Ahora es una espera por contenido, de 20 ms de granularidad. */
    char resp[SIM_AT_MAX_RESP_LEN];
    char *data = NULL;

    err = simcom_wait_resp_for("+CMQTTPUB:", (uint32_t)pub_timeout * 1000U);
    if (err != SIM_AT_OK)
    {
        ESP_LOGE(TAG, "No +CMQTTPUB URC received (%s)", simcom_err_to_str(err));
        return SIM_AT_ERR_RESPONSE;
    }

    simcom_responses_err_t resp_err = mqtt_read_urc("+CMQTTPUB", resp, &data);
    if (resp_err != SIM_AT_RESPONSE_OK)
    {
        ESP_LOGE(TAG, "No se pudo leer +CMQTTPUB: %s", simcom_resp_err_to_str(resp_err));
        return SIM_AT_ERR_RESPONSE;
    }

    int aux, err_code;
    if (sscanf(data, "%d,%d", &aux, &err_code) != 2)
        return SIM_AT_ERR_RESPONSE;

    if (err_code != SIM_MQTT_OK)
    {
        ESP_LOGE(TAG, "Error publishing MQTT message: %s", simcom_mqtt_err_to_str(err_code));
        return SIM_AT_ERR_RESPONSE;
    }

    return SIM_AT_OK;
}