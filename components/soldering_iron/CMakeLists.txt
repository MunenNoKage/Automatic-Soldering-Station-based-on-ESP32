# Реєструємо компонент паяльника
idf_component_register(

    # 1. Джерельні файли (.c та .cpp)
    # Цей рядок у вас вже був правильний
    SRCS "soldering_iron_hal.c"
         "SolderingIron.cpp"

    # 2. Публічна папка з заголовками (.h та .hpp)
    # Цей рядок також правильний
    INCLUDE_DIRS "include"

    # 3. Залежності (Ось тут виправлення)
    REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        log         # ПОТРІБНО: Надає 'esp_log.h' (використовується в HAL)
        esp_timer   # ПОТРІБНО: Надає 'esp_timer.h' (для ПІД-регулятора в HAL)
        esp_common  # ПОТРІБНО: Надає 'esp_err_t' та базові типи
        cxx         # ПОТРІБНО: Необхідно для компіляції 'SolderingIron.cpp'
        freertos    # РЕКОМЕНДОВАНО: Для C++ у середовищі RTOS
)