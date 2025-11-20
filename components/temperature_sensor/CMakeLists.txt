# Реєструємо компонент в системі збірки ESP-IDF
idf_component_register(

    # 1. Джерельні файли (.c та .cpp) для компіляції
    SRCS "temperature_sensor_hal.c"
         "TemperatureSensor.cpp"

    # 2. Публічна папка з заголовками (.h та .hpp)
    # Інші компоненти (напр. 'main') зможуть знайти файли звідси
    INCLUDE_DIRS "include"

    # 3. Залежності
    # Ми повідомляємо системі збірки, що цей компонент
    # потребує інші системні бібліотеки
    REQUIRES
          esp_driver_gpio
          esp_driver_spi
          driver        # Для gpio_num_t (driver/gpio.h)
          log           # Для HAL (esp_log.h)
          esp_common    # Для HAL (esp_err_t)
          cxx           # Необхідно для будь-якого C++ коду
          freertos      # Для C++ (ймовірно, для Task або Mutex)
)
