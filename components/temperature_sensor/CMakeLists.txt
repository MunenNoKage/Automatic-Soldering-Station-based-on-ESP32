# Register component in ESP-IDF build system
idf_component_register(

    # 1. Source files (.c and .cpp) for compilation
    SRCS "temperature_sensor_hal.c"
         "TemperatureSensor.cpp"

    # 2. Public folder with headers (.h and .hpp)
    # Other components (e.g. 'main') will be able to find files from here
    INCLUDE_DIRS "include"

    # 3. Dependencies
    # We inform the build system that this component
    # requires other system libraries
    REQUIRES
          esp_driver_gpio
          esp_driver_spi
          driver        # For gpio_num_t (driver/gpio.h)
          log           # For HAL (esp_log.h)
          esp_common    # For HAL (esp_err_t)
          cxx           # Necessary for any C++ code
          freertos      # For C++ (probably for Task or Mutex)
)
