idf_component_register(
    SRCS "execution_fsm.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        stepper_motor
        esp_timer
        freertos
        gcode_parser
)
