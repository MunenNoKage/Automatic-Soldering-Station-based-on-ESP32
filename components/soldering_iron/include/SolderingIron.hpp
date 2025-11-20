/**
 * @file SolderingIron.hpp
 * @brief C++ API wrapper for soldering iron control
 * 
 * Object-oriented interface for soldering iron temperature management.
 * Wraps the C HAL layer with modern C++ features.
 */

#ifndef SOLDERING_IRON_HPP
#define SOLDERING_IRON_HPP

#include "soldering_iron_hal.h"
#include <cstdint>

/**
 * @brief C++ wrapper class for soldering iron control
 */
class SolderingIron {
public:
    /**
     * @brief Constructor
     * @param config Soldering iron configuration
     */
    explicit SolderingIron(const soldering_iron_config_t& config);
    
    /**
     * @brief Destructor
     */
    ~SolderingIron();
    
    SolderingIron(const SolderingIron&) = delete;
    SolderingIron& operator=(const SolderingIron&) = delete;
    
    SolderingIron(SolderingIron&& other) noexcept;
    SolderingIron& operator=(SolderingIron&& other) noexcept;
    
    /**
     * @brief Set power output (0-100%)
     */
    void setPower(double duty_cycle);
    
    /**
     * @brief Set target temperature in Celsius
     */
    void setTargetTemperature(double temperature);
    
    /**
     * @brief Get target temperature
     */
    double getTargetTemperature() const;
    
    /**
     * @brief Update PID control loop
     */
    void updateControl(double current_temperature);
    
    /**
     * @brief Enable or disable heater
     */
    void setEnable(bool enable);
    
    /**
     * @brief Get current power output
     */
    double getPower() const;
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return handle_ != nullptr; }

    /**
     * @brief Встановлює нові константи ПІД-регулятора
     */
    void setPIDConstants(double kp, double ki, double kd);

    /**
     * @brief Отримує поточні константи ПІД-регулятора
     */
    void getPIDConstants(double &kp, double &ki, double &kd) const;

private:
    soldering_iron_handle_t handle_;
};

#endif // SOLDERING_IRON_HPP
