#ifndef _SIMCOM_CONFIG_H_
#define _SIMCOM_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"
#include "driver/gpio.h"

/**
 * -----------------------------
 * ----- [ Configuration ] -----
 * ----------------------------- 
 */

/**
 * Pins struct: use -1 for unused pins.
 */
typedef struct {
    gpio_num_t dtr_pin;     // DTR pin number (used to wake/sleep modem)
    gpio_num_t pwrkey_pin;  // PWRKEY pin (power on/off sequence)
    gpio_num_t rst_pin;     // RESET pin
} simcom_pins_t;

/**
 * Top-level config for init.
 *
 * Note: user-provided uart config is an esp-idf uart_config_t; pins are provided too.
 */
typedef struct {
    gpio_num_t tx_pin;                  // TX gpio
    gpio_num_t rx_pin;                  // RX gpio
    uart_port_t uart_port;              // UART port (e.g., UART_NUM_1)
    uart_config_t uart_conf;            // baud_rate, data_bits, parity, stop_bits, flow_ctrl...
    
    simcom_pins_t control_pins;         // DTR / PWRKEY / RESET wiring
    
    uint32_t default_cmd_timeout_ms;    // default blocking command timeout
    
    bool use_hw_flow_control;           // enable hardware flow control (RTS/CTS)
    gpio_num_t rts_pin;                 // RTS gpio (-1 if unused)
    gpio_num_t cts_pin;                 // CTS gpio (-1 if unused)
} simcom_config_t;

#ifdef __cplusplus
}
#endif

#endif // _SIMCOM_CONFIG_H_