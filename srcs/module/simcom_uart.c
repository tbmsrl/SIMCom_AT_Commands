#include "simcom.h"
#include "at/sim_at.h"

static const char* TAG = "simcom_uart";

static simcom_config_t g_cfg;
static bool g_inited = false;

/* Public API implementations */

simcom_err_t simcom_init(const simcom_config_t *cfg)
{
    if (!cfg)
        return SIM_AT_ERR_INVALID_ARG;
    if (g_inited)
        return SIM_AT_ERR_ABORTED;

    /* copy config */
    memcpy(&g_cfg, cfg, sizeof(g_cfg));
    simcom_set_config(&g_cfg);

    simcom_sem_create();

    /* uart config */
    esp_err_t e;
    e = uart_driver_install(g_cfg.uart_port, SIM_AT_MAX_RESP_LEN * 2, 0, 0, NULL, 0);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed: %d", e);
        return SIM_AT_ERR_UART;
    }
    e = uart_param_config(g_cfg.uart_port, &g_cfg.uart_conf);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed: %d", e);
        uart_driver_delete(g_cfg.uart_port);
        return SIM_AT_ERR_UART;
    }
    e = uart_set_pin(g_cfg.uart_port, g_cfg.tx_pin, g_cfg.rx_pin, g_cfg.rts_pin, g_cfg.cts_pin);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed: %d", e);
        uart_driver_delete(g_cfg.uart_port);
        return SIM_AT_ERR_UART;
    }
    ESP_LOGI(TAG, "UART port %d initialized on TX=%d, RX=%d", g_cfg.uart_port, g_cfg.tx_pin, g_cfg.rx_pin);

    // TODO: Controlar bien esto y hacerlo funcionar
    // /* configure control pins as outputs if set */
    // if (g_cfg.control_pins.dtr_pin >= 0) {
    //     gpio_set_direction(g_cfg.control_pins.dtr_pin, GPIO_MODE_OUTPUT);
    //     gpio_set_level(g_cfg.control_pins.dtr_pin, 0);
    // }
    // if (g_cfg.control_pins.pwrkey_pin >= 0) {
    //     gpio_set_direction(g_cfg.control_pins.pwrkey_pin, GPIO_MODE_OUTPUT);
    //     gpio_set_level(g_cfg.control_pins.pwrkey_pin, 0);
    // }
    // if (g_cfg.control_pins.rst_pin >= 0) {
    //     gpio_set_direction(g_cfg.control_pins.rst_pin, GPIO_MODE_OUTPUT);
    //     gpio_set_level(g_cfg.control_pins.rst_pin, 1); /* assume active-low reset */
    // }

    /* create parser task */
    BaseType_t ok = simcom_parser_task_create();
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "failed to create parser task");
        uart_driver_delete(g_cfg.uart_port);
        simcom_sem_delete();
        return SIM_AT_ERR_INTERNAL;
    }

    g_inited = true;

    // TODO: Ver si queda acá, o se pasa como argumento, o qué
    simcom_enable_debug(true);
    
    // TODO: Ver de mejorar esto? Capaz tendría que ir aparte
    // Además faltaría checkear que se haya podido sincronizar correctamente
    /* disable echo by default to simplify parsing (try but tolerate failure) */
    char resp[SIM_AT_MAX_RESP_LEN];
    simcom_cmd_sync("AT\r\n", 2000);
    simcom_ignore_resp();
    simcom_cmd_sync("ATE0\r\n", 2000);
    simcom_ignore_resp();

    ESP_LOGI(TAG, "sim_at initialized");
    simcom_set_init_flag(true);
    return SIM_AT_OK;
}

simcom_err_t simcom_deinit(void)
{
    if (!g_inited)
        return SIM_AT_ERR_NOT_INIT;
    
    simcom_parser_task_delete();

    /* delete uart driver */
    uart_driver_delete(g_cfg.uart_port);
    
    simcom_sem_delete();
    
    g_inited = false;
    ESP_LOGI(TAG, "sim_at deinitialized");
    simcom_set_init_flag(false);
    return SIM_AT_OK;
}

// TODO: Falta hacer y probar todas estas
// simcom_err_t sim_at_control_dtr(bool state)
// {
//     if (!g_inited)
//         return SIM_AT_ERR_NOT_INIT;
//     if (g_cfg.control_pins.dtr_pin < 0)
//         return SIM_AT_ERR_INVALID_ARG;
//     gpio_set_level(g_cfg.control_pins.dtr_pin, state ? 1 : 0);
//     return SIM_AT_OK;
// }

// simcom_err_t sim_at_control_pwrkey(bool state)
// {
//     if (!g_inited)
//         return SIM_AT_ERR_NOT_INIT;
//     if (g_cfg.control_pins.pwrkey_pin < 0)
//         return SIM_AT_ERR_INVALID_ARG;
//     gpio_set_level(g_cfg.control_pins.pwrkey_pin, state ? 1 : 0);
//     return SIM_AT_OK;
// }

// simcom_err_t sim_at_control_reset(bool state)
// {
//     if (!g_inited)
//         return SIM_AT_ERR_NOT_INIT;
//     if (g_cfg.control_pins.rst_pin < 0)
//         return SIM_AT_ERR_INVALID_ARG;
//     gpio_set_level(g_cfg.control_pins.rst_pin, state ? 1 : 0);
//     return SIM_AT_OK;
// }