/**
 * @file TemperatureSensor.cpp
 * @brief C++ implementation for MAX6675
 */

#include "TemperatureSensor.hpp"
#include "esp_log.h" // For logging errors
#include <cmath>     // For NAN (Not a Number)
#include <utility>   // For std::swap

// Tag for C++ logger
static const char *TAG = "TempSensorCPP";

// --- Constructor ---
TemperatureSensor::TemperatureSensor(const temperature_sensor_config_t &config)
    : handle_(nullptr) // Initialize handle_ as nullptr
{
    // Call C-HAL initialization function
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

// --- Destructor ---
TemperatureSensor::~TemperatureSensor()
{
    if (isInitialized())
    {
        // Call C-HAL deinitialization function
        temperature_sensor_hal_deinit(handle_);
        handle_ = nullptr; // Set to null for safety
    }
}

// --- Move constructor ---
// "Steals" C-handle from temporary object
TemperatureSensor::TemperatureSensor(TemperatureSensor &&other) noexcept
    : handle_(other.handle_)
{
    // Old object is now "empty" and its destructor
    // will safely do nothing
    other.handle_ = nullptr;
}

// --- Move assignment operator ---
TemperatureSensor &TemperatureSensor::operator=(TemperatureSensor &&other) noexcept
{
    if (this != &other)
    { // Protection from self-assignment

        // 1. First, free own resources if any
        if (isInitialized())
        {
            temperature_sensor_hal_deinit(handle_);
        }

        // 2. "Steal" the handle from the other object
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

// --- Implementation of readRaw ---
uint32_t TemperatureSensor::readRaw() const
{
    if (!isInitialized())
    {
        ESP_LOGE(TAG, "readRaw() called on uninitialized sensor");
        return 0; // Return 0 on error
    }

    uint16_t raw_data = 0;

    // Call our C-HAL function
    esp_err_t ret = temperature_sensor_hal_read_raw(handle_, &raw_data);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read raw data from C-HAL: %s", esp_err_to_name(ret));
        return 0; // Return 0 on error
    }

    // Cast to uint32_t as required by .hpp
    return static_cast<uint32_t>(raw_data);
}

// --- Implementation of readTemperature ---
double TemperatureSensor::readTemperature() const
{
    if (!isInitialized())
    {
        ESP_LOGE(TAG, "readTemperature() called on uninitialized sensor");
        return NAN; // NAN (Not a Number) - best way to report error
    }

    double temp = 0.0;

    // Call our C-HAL function
    esp_err_t ret = temperature_sensor_hal_read_temperature(handle_, &temp);

    // Handle return codes from C-HAL
    if (ret == ESP_OK)
    {
        return temp; // Success
    }

    if (ret == ESP_FAIL)
    {
        // ESP_FAIL - this is our "not connected" error from HAL
        ESP_LOGW(TAG, "Sensor is not connected (Open Circuit)");
        return NAN;
    }

    // Any other error (probably SPI)
    ESP_LOGE(TAG, "Failed to read temperature from C-HAL: %s", esp_err_to_name(ret));
    return NAN;
}