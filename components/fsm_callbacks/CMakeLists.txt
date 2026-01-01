idf_component_register(
    SRCS "fsm_callbacks.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        freertos
        esp_timer
        fsm_controller
        stepper_motor
        soldering_iron
        temperature_sensor
        execution_fsm
)
