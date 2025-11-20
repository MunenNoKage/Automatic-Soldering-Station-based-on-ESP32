/**
 * @file TemperatureSensor.cpp
 * @brief C++ реалізація для MAX6675
 */

#include "TemperatureSensor.hpp"
#include "esp_log.h" // Для логування помилок
#include <cmath>     // Для NAN (Not a Number)
#include <utility>   // Для std::swap

// Тег для C++ логера
static const char *TAG = "TempSensorCPP";

// --- Конструктор ---
TemperatureSensor::TemperatureSensor(const temperature_sensor_config_t &config)
    : handle_(nullptr) // Ініціалізуємо handle_ як nullptr
{
    // Викликаємо C-HAL функцію ініціалізації
    handle_ = temperature_sensor_hal_init(&config);

    if (!isInitialized())
    {
        ESP_LOGE(TAG, "Failed to initialize C-HAL for temperature sensor");
    }
    else
    {
        ESP_LOGI(TAG, "C++ TemperatureSensor initialized");
    }
}

// --- Деструктор ---
TemperatureSensor::~TemperatureSensor()
{
    if (isInitialized())
    {
        // Викликаємо C-HAL функцію деініціалізації
        temperature_sensor_hal_deinit(handle_);
        handle_ = nullptr; // Встановлюємо в null для безпеки
    }
}

// --- Конструктор переміщення ---
// "Викрадає" C-handle у тимчасового об'єкта
TemperatureSensor::TemperatureSensor(TemperatureSensor &&other) noexcept
    : handle_(other.handle_)
{
    // Старий об'єкт тепер "порожній" і його деструктор
    // безпечно нічого не зробить
    other.handle_ = nullptr;
}

// --- Оператор присвоєння переміщення ---
TemperatureSensor &TemperatureSensor::operator=(TemperatureSensor &&other) noexcept
{
    if (this != &other)
    { // Захист від самоприсвоєння

        // 1. Спочатку звільняємо власні ресурси, якщо вони є
        if (isInitialized())
        {
            temperature_sensor_hal_deinit(handle_);
        }

        // 2. "Викрадаємо" ручку в іншого об'єкта
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

// --- Реалізація readRaw ---
uint32_t TemperatureSensor::readRaw() const
{
    if (!isInitialized())
    {
        ESP_LOGE(TAG, "readRaw() called on uninitialized sensor");
        return 0; // Повертаємо 0 у разі помилки
    }

    uint16_t raw_data = 0;

    // Викликаємо нашу C-HAL функцію
    esp_err_t ret = temperature_sensor_hal_read_raw(handle_, &raw_data);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read raw data from C-HAL: %s", esp_err_to_name(ret));
        return 0; // Повертаємо 0 у разі помилки
    }

    // Приводимо до типу uint32_t, як вимагає .hpp
    return static_cast<uint32_t>(raw_data);
}

// --- Реалізація readTemperature ---
double TemperatureSensor::readTemperature() const
{
    if (!isInitialized())
    {
        ESP_LOGE(TAG, "readTemperature() called on uninitialized sensor");
        return NAN; // NAN (Not a Number) - найкращий спосіб повідомити про помилку
    }

    double temp = 0.0;

    // Викликаємо нашу C-HAL функцію
    esp_err_t ret = temperature_sensor_hal_read_temperature(handle_, &temp);

    // Обробляємо коди повернення з C-HAL
    if (ret == ESP_OK)
    {
        return temp; // Успіх
    }

    if (ret == ESP_FAIL)
    {
        // ESP_FAIL - це наша помилка "не підключено" з HAL
        ESP_LOGW(TAG, "Sensor is not connected (Open Circuit)");
        return NAN;
    }

    // Будь-яка інша помилка (ймовірно, SPI)
    ESP_LOGE(TAG, "Failed to read temperature from C-HAL: %s", esp_err_to_name(ret));
    return NAN;
}