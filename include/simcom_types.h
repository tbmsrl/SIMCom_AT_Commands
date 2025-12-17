#ifndef _SIMCOM_TYPES_H_
#define _SIMCOM_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ------------------------------------
 * ----- [ Error / status codes ] -----
 * ------------------------------------ 
 */

 typedef enum {
    SIM_AT_OK = 0,
    SIM_AT_ERR_INVALID_ARG = -1,
    SIM_AT_ERR_NO_MEM = -2,    /* reserved—shouldn't be returned by API since no dynamic alloc */
    SIMCOM_ERR_TIMEOUT = -3,
    SIM_AT_ERR_UART = -4,
    SIM_AT_ERR_BUSY = -5,
    SIM_AT_ERR_INTERNAL = -6,
    SIM_AT_ERR_NOT_INIT = -7,
    SIM_AT_ERR_OVERFLOW = -8,  /* response too long for provided buffer */
    SIM_AT_ERR_ABORTED = -9,
    SIM_AT_ERR_RESPONSE = -10,
} simcom_err_t;
// TODO: Completar con los errores que faltan capaz?

typedef enum {
    SIM_AT_RESPONSE_OK = 0,
    SIM_AT_RESPONSE_COMMAND_OK = -1,
    SIM_AT_RESPONSE_ERR_INVALID_FORMAT = -2,
    SIM_AT_RESPONSE_ERR_COMMAND_ERROR = -3,
    SIM_AT_RESPONSE_ERR_COMMAND_INVALID = -4,
} simcom_responses_err_t;

/**
 * ------------------------------------
 * ----- [ Status control codes ] -----
 * ------------------------------------ 
 */

 typedef enum {
    FUN_MINIMUN_FUNCTIONALITY = 0,
    FUN_FULL_FUNCTIONALITY = 1,
    FUN_DISABLE_PHONE = 4,
    FUN_FACTORY_TEST_MODE = 5,
    FUN_RESET = 6,
    FUN_OFFLINE_MODE = 7,
    FUN_DISABLE_SIM = 8,
} sim_status_control_fun_t;

 /**
 * ------------------------------------------
 * ----- [ Network registration codes ] -----
 * ------------------------------------------ 
 */

typedef enum {
    NOT_REGISTERED = 0,
    REGISTERED,
    SEARCHING,
    REGISTRATION_DENIED,
    UNKNOWN,
    ROAMING,
    SMS_ONLY,
    SMS_ONLY_ROAMING,
    EMERGENCY = 11,
} sim_network_registration_stat_t;

/**
 * ------------------------------------------
 * ----- [ Packet Domain status codes ] -----
 * ------------------------------------------ 
 */

typedef enum {
    EPS_NOT_REGISTERED = 0,
    EPS_REGISTERED,
    EPS_SEARCHING,
    EPS_REGISTRATION_DENIED,
    EPS_UNKNOWN,
    EPS_ROAMING,
    EPS_SMS_ONLY,
    EPS_SMS_ONLY_ROAMING,
    EPS_EMERGENCY = 11,
} sim_eps_network_registration_stat_t;

typedef enum {
    PDP_IP = 0,
    PDP_IPV6, 
    PDP_IPV4V6,
} sim_pdp_type_t;

/**
 * ----------------------------------
 * ----- [ SIM Card Pin codes ] -----
 * ---------------------------------- 
 */

typedef enum {
    SIMCARD_READY = 0,
    SIMCARD_SIM_PIN,
    SIMCARD_SIM_PUK,
    SIMCARD_PH_SIM_PIN,
    SIMCARD_SIM_PIN_2,
    SIMCARD_SIM_PUK_2,
    SIMCARD_PH_NET_PIN,
} sim_simcard_pin_code_t;

/**
 * -------------------------------
 * ----- [ NTP error codes ] -----
 * ------------------------------- 
 */

typedef enum {
    NTP_OPERATION_SUCCEEDED = 0,
    NTP_UNKOWNW_ERROR = 1,
    NTP_WRONG_PARAMETER = 2,
    NTP_WRONG_DATE_AND_TIME_CALCULATED = 3,
    NTP_NETWORK_ERROR = 4,
    NTP_TIMEZONE_ERROR = 5,
    NTP_TIMEOUT_ERROR = 6,
} sim_at_ntp_err_code_t;

/**
 * --------------------------------
 * ----- [ MQTT error codes ] -----
 * -------------------------------- 
 */

typedef enum {
    SIM_MQTT_OK                                 = 0,  // Operation succeeded
    SIM_MQTT_ERR_FAIL                           = 1,  // General failure
    SIM_MQTT_ERR_BAD_UTF8_STRING                = 2,  // Bad UTF-8 string
    SIM_MQTT_ERR_SOCK_CONNECT_FAIL              = 3,  // Socket connect failed
    SIM_MQTT_ERR_SOCK_CREATE_FAIL               = 4,  // Socket creation failed
    SIM_MQTT_ERR_SOCK_CLOSE_FAIL                = 5,  // Socket close failed
    SIM_MQTT_ERR_MSG_RECEIVE_FAIL               = 6,  // Message receive failed
    SIM_MQTT_ERR_NETWORK_OPEN_FAIL              = 7,  // Network open failed
    SIM_MQTT_ERR_NETWORK_CLOSE_FAIL             = 8,  // Network close failed
    SIM_MQTT_ERR_NETWORK_NOT_OPENED             = 9,  // Network not opened
    SIM_MQTT_ERR_CLIENT_INDEX                   = 10, // Invalid client index
    SIM_MQTT_ERR_NO_CONNECTION                  = 11, // No active connection
    SIM_MQTT_ERR_INVALID_PARAMETER              = 12, // Invalid parameter
    SIM_MQTT_ERR_NOT_SUPPORTED                  = 13, // Operation not supported
    SIM_MQTT_ERR_CLIENT_BUSY                    = 14, // Client is busy
    SIM_MQTT_ERR_REQUIRE_CONN_FAIL              = 15, // Require connection failed
    SIM_MQTT_ERR_SOCK_SEND_FAIL                 = 16, // Socket sending failed
    SIM_MQTT_ERR_TIMEOUT                        = 17, // Timeout occurred
    SIM_MQTT_ERR_TOPIC_EMPTY                    = 18, // Topic is empty
    SIM_MQTT_ERR_CLIENT_IN_USE                  = 19, // Client is already used
    SIM_MQTT_ERR_CLIENT_NOT_ACQUIRED            = 20, // Client not acquired
    SIM_MQTT_ERR_CLIENT_NOT_RELEASED            = 21, // Client not released
    SIM_MQTT_ERR_LEN_OUT_OF_RANGE               = 22, // Length out of range
    SIM_MQTT_ERR_NETWORK_OPENED                 = 23, // Network already opened
    SIM_MQTT_ERR_PACKET_FAIL                    = 24, // Packet failure
    SIM_MQTT_ERR_DNS                            = 25, // DNS error
    SIM_MQTT_ERR_SOCK_CLOSED_BY_SERVER          = 26, // Socket closed by server
    SIM_MQTT_ERR_CONN_REFUSED_BAD_PROTOCOL      = 27, // Connection refused: unaccepted protocol version
    SIM_MQTT_ERR_CONN_REFUSED_ID_REJECTED       = 28, // Connection refused: identifier rejected
    SIM_MQTT_ERR_CONN_REFUSED_SERVER_UNAVAILABLE= 29, // Connection refused: server unavailable
    SIM_MQTT_ERR_CONN_REFUSED_BAD_CREDENTIALS   = 30, // Connection refused: bad user name or password
    SIM_MQTT_ERR_CONN_REFUSED_NOT_AUTHORIZED    = 31, // Connection refused: not authorized
    SIM_MQTT_ERR_HANDSHAKE_FAIL                 = 32, // Handshake failed
    SIM_MQTT_ERR_CERT_NOT_SET                   = 33, // Certificate not set
    SIM_MQTT_ERR_OPEN_SESSION_FAIL              = 34, // Open session failed
    SIM_MQTT_ERR_DISCONNECT_FAIL                = 35  // Disconnect from server failed
} sim_mqtt_err_codes_t;

#ifdef __cplusplus
}
#endif

#endif // _SIMCOM_TYPES_H_
