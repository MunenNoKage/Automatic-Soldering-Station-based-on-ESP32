idf_component_register(
    SRCS "system_config.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        freertos
        esp_timer
        stepper_motor
        soldering_iron
        temperature_sensor
        fsm_controller
        fsm_callbacks
        execution_fsm
        web_server
        wifi_manager
)
