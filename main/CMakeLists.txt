# CMakeLists.txt
# Build configuration for main application

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        stepper_motor
        fsm_controller
        execution_fsm
        esp_timer
        wifi_manager
        web_server
        soldering_iron
        temperature_sensor
)
