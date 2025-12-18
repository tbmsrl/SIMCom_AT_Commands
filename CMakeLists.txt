set(srcs
	srcs/at/sim_at.c
    srcs/module/simcom_uart.c
    srcs/services/sim_basic_at.c
    srcs/services/sim_status_control_at.c
    srcs/services/sim_network_at.c
    srcs/services/sim_simcard_at.c
    srcs/services/sim_packet_domain_at.c
    srcs/services/sim_internet_services_at.c 
    srcs/services/sim_mqtt_at.c 
)

idf_component_register(
    SRCS ${srcs} 
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "srcs"
    REQUIRES driver
)
