#ifndef _SIMCOM_H_
#define _SIMCOM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "string.h"

#include "esp_log.h"

#include "simcom_types.h"
#include "simcom_config.h"

/**
 * -----------------------------------
 * ----- [ Core API: lifecycle ] -----
 * ----------------------------------- 
 */

/**
 * @brief Initialize the SIM AT core library.
 * - This function configures UART, creates internal static resources and starts the parser task.
 * - No dynamic allocation is performed for public structures; internal static buffers are allocated inside the implementation.
 *
 * @param cfg Configuration struct pointer
 *
 * @return 
 *  - SIM_AT_OK on success.
 *  - SIM_AT_ERR_ABORTED API already initialized
 *  - SIM_AT_ERR_INVALID_ARG on empty config 
 *  - SIM_AT_ERR_NO_MEM no memory available
 *  - SIM_AT_ERR_UART error initializing UART
 *  - SIM_AT_ERR_INTERNAL error creating parser task
 */
simcom_err_t simcom_init(const simcom_config_t *cfg);

/**
 * @brief Deinitialize library. 
 * Stops parser task and release internal resources.
 * Safe to call multiple times. After this, library functions return SIM_AT_ERR_NOT_INIT.
 * 
 * @return 
 *  - SIM_AT_OK on success.
 *  - SIM_AT_ERR_NOT_INIT if the API is not initialized
 */
simcom_err_t simcom_deinit(void);

/* ================================================== */
/* =============== [ Status Control ] =============== */
/* ================================================== */

/**
 * [--- List of available commands ---]
 * 
 * [x] AT+CFUN      = Set phone functionality
 * [x] AT+CSQ       = Query signal quality
 * [ ] AT+AUTOCSQ   = Set CSQ report
 * [ ] AT+CSQDELTA  = Set RSSI delta change threshold
 * [x] AT+CPOF      = Power down the module
 * [x] AT+CRESET    = Reset the module
 * [ ] AT+CACM      = Accumulated call meter
 * [ ] AT+CAMM      = Accumulated call meter maximum
 * [ ] AT+CPUC      = Price per unit and currency table
 * [x] AT+CCLK      = Real time clock management
 * [ ] AT+CMEE      = Report mobile equipment error
 * [ ] AT+CPAS      = Phone activity status
 * [ ] AT+SIMEI     = Set IMEI for the module
 * 
 */

simcom_err_t simcom_comm_test(void);

/**
 * @brief Returns the current phone functionality
 * 
 * @param fun Phone functinality status code:
 *  - MINIMUN_FUNCTIONALITY - minimum functionality
 *  - FULL_FUNCTIONALITY    - full functionality, online mode
 *  - DISABLE_PHONE         - disable phone both transmit and receive RF circuits
 *  - FACTORY_TEST_MODE     - Factory Test Mode
 *  - RESET                 - Reset
 *  - OFFLINE_MODE          - Offline Mode
 *  - DISABLE_SIM           - Disable SIM
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_get_phone_func(sim_status_control_fun_t* fun);

/**
 * @brief Sets the current phone functionality
 * 
 * @param fun Phone functinality status code:
 *  - MINIMUN_FUNCTIONALITY - minimum functionality
 *  - FULL_FUNCTIONALITY    - full functionality, online mode
 *  - DISABLE_PHONE         - disable phone both transmit and receive RF circuits
 *  - FACTORY_TEST_MODE     - Factory Test Mode
 *  - RESET                 - Reset
 *  - OFFLINE_MODE          - Offline Mode
 *  - DISABLE_SIM           - Disable SIM
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_set_phone_func(sim_status_control_fun_t fun);

/**
 * @brief Gets the signal strength <rssi> and the channel bit error rate <ber> 
 * rssi:
 *  - 0 -113 dBm or less
 *  - 1 -111 dBm
 *  - 2…30 -109… -53 dBm
 *  - 31 -51 dBm or greater
 *  - 99 not known or not detectable
 * 
 * ber:
 *  - 0 <0.01%
 *  - 1 0.01% --- 0.1%
 *  - 2 0.1% --- 0.5%
 *  - 3 0.5% --- 1.0%
 *  - 4 1.0% --- 2.0%
 *  - 5 2.0% --- 4.0%
 *  - 6 4.0% --- 8.0%
 *  - 7 >=8.0%
 *  - 99 not known or not detectable
 * 
 * @param rssi Signal strength
 * @param ber Bit error rate 
 *      
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_query_signal_quality(int* rssi, int* ber);

/**
 * @brief Parse the +QSC command response to dBm
 * 
 * @param rssi QSC <rssi> command response
 * 
 * @return dBm value
 */
int simcom_rssi_to_dbm(int rssi);

/**
 * @brief Parse the +QSC bit error rate to string
 * 
 * @param ber QSC <ber> response
 * 
 * @return A string with the bit error rate percentage
 */
const char* simcom_ber_to_str(int ber);

/**
 * @brief Power downs sim module
 * 
 * @return SIM_AT_OK if succeded
 */
simcom_err_t simcom_power_down_module(void);

/**
 * @brief Resets sim module
 * 
 * @return SIM_AT_OK if succeded
 */
simcom_err_t simcom_reset_module(void);

/**
 * @brief Get current RTC time in the "yy/MM/dd,hh:mm:ss±zz" format
 * 
 * @param rtc_time String where the RTC time will be stored
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_get_rtc_time(char* rtc_time);


/* =========================================== */
/* =============== [ Network ] =============== */
/* =========================================== */

/**
 * [--- List of available commands ---]
 * 
 * [x] AT+CREG          = Network registration
 * [ ] AT+COPS          = Operator selection
 * [ ] AT+CUSD          = Unstructured supplementary service data
 * [ ] AT+CSSN          = Supplementary service notifications
 * [ ] AT+CPOL          = Preferred operator list
 * [ ] AT+COPN          = Read operator names
 * [ ] AT+CNMP          = Preferred mode selection
 * [ ] AT+CNBP          = Preferred band selection
 * [ ] AT+CPSI          = Inquiring UE system information
 * [ ] AT+CNSMOD        = Show network system mode
 * [ ] AT+CTZU          = Automatic time and time zone update
 * [ ] AT+CTZR          = Time and time zone reporting
 * 
 */

/**
 * @brief Returns the status of result code presentation and an integer <stat> which shows whether
 * the network has currently indicated the registration of the ME.
 * 
 * @param stat Status code
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_net_reg(sim_network_registration_stat_t* stat);

/**
 * @brief Parse the +REG status code to string
 * 
 * @param stat Status code
 * 
 * @return A string with the status code description
 */
const char* simcom_net_stat_to_str(sim_network_registration_stat_t stat);

// TODO: COPS por el momento no se utiliza, pero podría ser necesaria


/* ================================================= */
/* =============== [ Packet Domain ] =============== */
/* ================================================= */

/**
 * [--- List of available commands ---]
 * 
 * [ ] AT+CGERG         = Network registration status
 * [x] AT+CEREG         = EPS network registration status
 * [x] AT+CGATT         = Packet domain attach or detach
 * [x] AT+CGACT         = PDP context activate or deactivate
 * [x] AT+CGDCONT       = Define PDP context
 * [ ] AT+CGDSCONT      = Define Secondary PDP Context
 * [ ] AT+CGTFT         = Traffic Flow Template
 * [ ] AT+CGQREQ        = Quality of service profile (requested)
 * [ ] AT+CGEQREQ       = 3G quality of service profile (requested)
 * [ ] AT+CGQMIN        = Quality of service profile (minimum acceptable)
 * [ ] AT+CGEQMIN       = 3G quality of service profile (minimum acceptable)
 * [ ] AT+CGDATA        = Enter data state
 * [x] AT+CGPADDR       = Show PDP address
 * [ ] AT+CGCLASS       = GPRS mobile station class
 * [ ] AT+CGEREP        = GPRS event reporting
 * [ ] AT+CGAUTH        = Set type of authentication for PDP-IP connections of GPRS
 * [x] AT+CPING         = Ping destination address
 * 
 */

/**
 * @brief Returns the status of result code presentation which shows whether the network has currently 
 * indicated the registration of the MT.
 * EPS (Evolved Packet System) is the core network architecture used in 4G LTE. EPS consists of two main parts: 
 * the E-UTRAN (radio access network, managed by eNodeBs) and the Evolved Packet Core (EPC), which handles 
 * user sessions, authentication, mobility, and internet access. 
 * 
 * @param stat Network registration status code
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_eps_net_reg(sim_eps_network_registration_stat_t* stat);

/**
 * @brief Parse the +CGREG status code to string
 * 
 * @param stat Status code
 * 
 * @return A string with the status code description
 */
const char* simcom_sim_eps_net_stat_to_str(sim_eps_network_registration_stat_t stat);

/**
 * @brief Gets the packet domain service state
 * 
 * @param state Indicates the state of Packet Domain attachment
 *  - 0 detached
 *  - 1 attached
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_get_packet_domain_attach(int* state);

/**
 * @brief Sets the packet domain service state
 * 
 * @param state Indicates the state of Packet Domain attachment
 *  - 0 detached
 *  - 1 attached
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_set_packet_domain_attach(int state);

/**
 * @brief Gets the state of a particular PDP context.
 * A PDP (Packet Data Protocol) context is a data structure in mobile networks that defines a user’s session 
 * for packet-switched data services. It contains parameters such as the user’s IP address, QoS (Quality of Service), 
 * and routing information. When a PDP context is activated, it establishes a logical connection between the user’s 
 * device and the network’s gateway, enabling internet or data access.
 * 
 * @param cid A numeric parameter which specifies a particular PDP context definition
 * @param state Indicates the state of PDP context activation
 *  - 0 deactivated
 *  - 1 activated
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_get_pdp_context_activate(int* cid, int* state);

/**
 * @brief Sets the state of a particular PDP context.
 * A PDP (Packet Data Protocol) context is a data structure in mobile networks that defines a user’s session 
 * for packet-switched data services. It contains parameters such as the user’s IP address, QoS (Quality of Service), 
 * and routing information. When a PDP context is activated, it establishes a logical connection between the user’s 
 * device and the network’s gateway, enabling internet or data access.
 * 
 * @param cid A numeric parameter which specifies a particular PDP context definition
 * @param state Indicates the state of PDP context activation
 *  - 0 deactivated
 *  - 1 activated
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_set_pdp_context_activate(int cid, int state);

/**
 * @brief Get the specified PDP context parameter values for a PDP context identified by the (local)context 
 * identification parameter.
 * Just logs the result in the console.
 * 
 * @returns SIM_AT_OK if succeded, Error Code if failed
 */
simcom_err_t simcom_get_pdp_context(void);

/**
 * @brief Specifies PDP context parameter values for a PDP context identified by the (local)context 
 * identification parameter.
 * 
 * @param cid A numeric parameter which specifies a particular PDP context definition
 * @param pdp_type A string parameter which specifies the type of packet data protocol
 * @param apn A string parameter which is a logical name that is used to select the GGSN or the external packet data network.
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed 
 */
simcom_err_t simcom_set_pdp_context(int cid, sim_pdp_type_t pdp_type, const char* apn);

/**
 * @brief Parse the PDP type to string
 * 
 * @param pdp_type PDP type
 * 
 * @return A string with the PDP type
 */
const char* simcom_pdp_type_to_str(sim_pdp_type_t pdp_type);

/**
 * @brief Returns a list of PDP addresses for the specified context identifiers
 * 
 * @param cid A numeric parameter which specifies a particular PDP context definition
 * @param addr A string that identifies the MT in the address space applicable to the PDP
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed 
 */
simcom_err_t simcom_show_pdp_addr(int* cid, char* addr);

/**
 * @brief Ping destination address.
 * 
 * @param dest_addr The destination is to be pinged; it can be an IP address or a domain name.
 * 
 * @return SIM_AT_OK if succeded, Error Code if failed 
 */
simcom_err_t simcom_ping(const char* dest_addr);


/* ============================================ */
/* =============== [ SIM Card ] =============== */
/* ============================================ */

/**
 * [--- List of available commands ---]
 * 
 * [ ] AT+CICCID                = Read ICCID from SIM card
 * [x] AT+CPIN                  = Enter PIN
 * [ ] AT+CLCK                  = Facility lock
 * [ ] AT+CPWD                  = Change password
 * [ ] AT+CIMI                  = Request international mobile subscriber identity
 * [ ] AT+CSIM                  = Generic SIM access
 * [ ] AT+CRSM                  = Restricted SIM access
 * [ ] AT+SPIC                  = Times remain to input SIM PIN/PUK
 * [ ] AT+CSPN                  = Get service provider name from SIM
 * [ ] AT+UIMHOTSWAPON          = Set UIM hotswap function on
 * [ ] AT+UIMHOTSWAPLEVEL       = Set UIM card detection level
 * [ ] AT+SWITCHSIM             = Switch master SIM
 * [ ] AT+DUALSIM               = Set dual-sim mode
 * [ ] AT+BINDSIM               = Bind ATP to SIM1 or SIM2
 * [ ] AT+DUALSIMURC            = Dual card reporting control
 * 
 */

 /**
  * @brief Get if the SIM Card needs a PIN to be initialized
  * 
  * @param code SIM Card Pin code:
  * - SIMCARD_READY: ME is not pending for any password
  * - SIMCARD_SIM_PIN: ME is waiting SIM PIN to be given
  * - SIMCARD_SIM_PUK: ME is waiting SIM PUK to be given
  * - SIMCARD_PH_SIM_PIN: ME is waiting phone-to-SIM card password to be given
  * - SIMCARD_SIM_PIN_2: ME is waiting SIM PIN2 to be given
  * - SIMCARD_SIM_PUK_2: ME is waiting SIM PUK2 to be given
  * - SIMCARD_PH_NET_PIN: ME is waiting network personalization password to be given
  * 
  * @return SIM_AT_OK if succeded, Error Code if failed
  */
simcom_err_t simcom_get_simcard_pin_info(sim_simcard_pin_code_t* code);

#ifdef __cplusplus
}
#endif

#endif // _SIMCOM_H_
