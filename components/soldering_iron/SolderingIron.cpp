/**
 * @file SolderingIron.cpp
 * @brief C++ реалізація для SolderingIron
 */

#include "SolderingIron.hpp"
#include "esp_log.h"
#include <utility> // Для std::swap

static const char *TAG = "SolderingIronCPP";

// --- Конструктор ---
SolderingIron::SolderingIron(const soldering_iron_config_t &config)
    : handle_(nullptr)
{
    // Викликаємо C-HAL для ініціалізації
    handle_ = soldering_iron_hal_init(&config);
    if (!isInitialized())
    {
        ESP_LOGE(TAG, "Failed to initialize soldering_iron_hal");
    }
    else
    {
        ESP_LOGI(TAG, "SolderingIron C++ object initialized");
    }
}

// --- Деструктор ---
SolderingIron::~SolderingIron()
{
    if (isInitialized())
    {
        // Викликаємо C-HAL для деініціалізації
        soldering_iron_hal_deinit(handle_);
        handle_ = nullptr;
    }
}

// --- Move Constructor ---
SolderingIron::SolderingIron(SolderingIron &&other) noexcept
    : handle_(other.handle_)
{
    other.handle_ = nullptr;
}

// --- Move Assignment Operator ---
SolderingIron &SolderingIron::operator=(SolderingIron &&other) noexcept
{
    if (this != &other)
    {
        if (isInitialized())
        {
            soldering_iron_hal_deinit(handle_);
        }
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

// --- Реалізація методів ---

void SolderingIron::setPower(double duty_cycle)
{
    if (!isInitialized())
        return;
    soldering_iron_hal_set_power(handle_, duty_cycle);
}

void SolderingIron::setTargetTemperature(double temperature)
{
    if (!isInitialized())
        return;
    soldering_iron_hal_set_target_temperature(handle_, temperature);
}

double SolderingIron::getTargetTemperature() const
{
    if (!isInitialized())
        return 0.0;
    return soldering_iron_hal_get_target_temperature(handle_);
}

void SolderingIron::updateControl(double current_temperature)
{
    if (!isInitialized())
        return;
    soldering_iron_hal_update_control(handle_, current_temperature);
}

void SolderingIron::setEnable(bool enable)
{
    if (!isInitialized())
        return;
    soldering_iron_hal_set_enable(handle_, enable);
}

double SolderingIron::getPower() const
{
    if (!isInitialized())
        return 0.0;
    return soldering_iron_hal_get_power(handle_);
}

// --- Реалізація нових ПІД-функцій ---

void SolderingIron::setPIDConstants(double kp, double ki, double kd)
{
    if (!isInitialized())
        return;

    // Примітка: ця функція ще не існує у вашому C-HAL.
    // Див. крок 3 нижче.
    soldering_iron_hal_set_pid_constants(handle_, kp, ki, kd);
}

void SolderingIron::getPIDConstants(double &kp, double &ki, double &kd) const
{
    if (!isInitialized())
    {
        kp = 0.0;
        ki = 0.0;
        kd = 0.0;
        return;
    }

    // Примітка: ця функція ще не існує у вашому C-HAL.
    // Див. крок 3 нижче.
    soldering_iron_hal_get_pid_constants(handle_, &kp, &ki, &kd);
}