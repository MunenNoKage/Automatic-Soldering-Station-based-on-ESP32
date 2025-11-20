# CMakeLists.txt
# Build configuration for stepper motor component

idf_component_register(
    SRCS 
        "stepper_motor_hal.c"
        "StepperMotor.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        esp_driver_gpio
        freertos
        esp_system
    PRIV_REQUIRES 
        driver 
        esp_timer
)
