# Register soldering iron component
idf_component_register(

    # 1. Source files (.c and .cpp)
    # This line was already correct for you
    SRCS "soldering_iron_hal.c"
         "SolderingIron.cpp"

    # 2. Public folder with headers (.h and .hpp)
    # This line is also correct
    INCLUDE_DIRS "include"

    # 3. Dependencies (Here's the fix)
    REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        log         # REQUIRED: Provides 'esp_log.h' (used in HAL)
        esp_timer   # REQUIRED: Provides 'esp_timer.h' (for PID controller in HAL)
        esp_common  # REQUIRED: Provides 'esp_err_t' and basic types
        cxx         # REQUIRED: Necessary for compiling 'SolderingIron.cpp'
        freertos    # RECOMMENDED: For C++ in RTOS environment
)