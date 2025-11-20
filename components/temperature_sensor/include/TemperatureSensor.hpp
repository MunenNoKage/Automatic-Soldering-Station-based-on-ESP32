/**
 * @file TemperatureSensor.hpp
 * @brief C++ API wrapper for temperature sensor
 * 
 * Object-oriented interface for temperature measurement.
 */

#ifndef TEMPERATURE_SENSOR_HPP
#define TEMPERATURE_SENSOR_HPP

#include "temperature_sensor_hal.h"
#include <cstdint>

/**
 * @brief C++ wrapper for temperature sensor
 */
class TemperatureSensor {
public:
    /**
     * @brief Constructor
     * @param config Sensor configuration
     */
    explicit TemperatureSensor(const temperature_sensor_config_t& config);
    
    /**
     * @brief Destructor
     */
    ~TemperatureSensor();
    
    TemperatureSensor(const TemperatureSensor&) = delete;
    TemperatureSensor& operator=(const TemperatureSensor&) = delete;
    
    TemperatureSensor(TemperatureSensor&& other) noexcept;
    TemperatureSensor& operator=(TemperatureSensor&& other) noexcept;
    
    /**
     * @brief Read raw ADC value
     */
    uint32_t readRaw() const;

    /**
     * @brief Read temperature in Celsius
     */
    double readTemperature() const;

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return handle_ != nullptr; }

private:
    temperature_sensor_handle_t handle_;
};

#endif // TEMPERATURE_SENSOR_HPP
