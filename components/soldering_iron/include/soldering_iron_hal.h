/**
 * @file soldering_iron_hal.h
 * @brief Hardware Abstraction Layer for soldering iron control
 * 
 * Provides low-level C interface for controlling soldering iron heating element.
 * Uses PWM signal to drive IRLZ44N MOSFET for temperature regulation.
 * Interfaces with temperature sensor for closed-loop control.
 */

#ifndef SOLDERING_IRON_HAL_H
#define SOLDERING_IRON_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Soldering iron configuration structure
 */
typedef struct {
    gpio_num_t heater_pwm_pin;
    ledc_timer_t pwm_timer;
    ledc_channel_t pwm_channel;
    uint32_t pwm_frequency;
    ledc_timer_bit_t pwm_resolution;
    double max_temperature;
    double min_temperature;
} soldering_iron_config_t;

/**
 * @brief Soldering iron handle
 */
typedef struct soldering_iron_handle_s* soldering_iron_handle_t;

/**
 * @brief Initialize soldering iron driver
 */
soldering_iron_handle_t soldering_iron_hal_init(const soldering_iron_config_t* config);

/**
 * @brief Deinitialize soldering iron driver
 */
void soldering_iron_hal_deinit(soldering_iron_handle_t handle);

/**
 * @brief Set PWM duty cycle (0-100%)
 */
void soldering_iron_hal_set_power(soldering_iron_handle_t handle, double duty_cycle);

/**
 * @brief Set target temperature
 */
void soldering_iron_hal_set_target_temperature(soldering_iron_handle_t handle, double temperature);

/**
 * @brief Get target temperature
 */
double soldering_iron_hal_get_target_temperature(soldering_iron_handle_t handle);

/**
 * @brief Update temperature control loop (call periodically)
 */
void soldering_iron_hal_update_control(soldering_iron_handle_t handle, double current_temperature);

/**
 * @brief Enable or disable heating
 */
void soldering_iron_hal_set_enable(soldering_iron_handle_t handle, bool enable);

/**
 * @brief Get current power output percentage
 */
double soldering_iron_hal_get_power(soldering_iron_handle_t handle);

/**
 * @brief Get current power output percentage
 */
double soldering_iron_hal_get_power(soldering_iron_handle_t handle);

/**
 * @brief Встановлює нові константи ПІД-регулятора
 * @param kp Пропорційний коефіцієнт
 * @param ki Інтегральний коефіцієнт
 * @param kd Диференціальний коефіцієнт
 */
void soldering_iron_hal_set_pid_constants(soldering_iron_handle_t handle, double kp, double ki, double kd);

/**
 * @brief Отримує поточні константи ПІД-регулятора
 */
void soldering_iron_hal_get_pid_constants(soldering_iron_handle_t handle, double *kp, double *ki, double *kd);

void soldering_iron_hal_set_pid_constants(soldering_iron_handle_t handle, double kp, double ki, double kd);

void soldering_iron_hal_get_pid_constants(soldering_iron_handle_t handle, double *kp, double *ki, double *kd);

#ifdef __cplusplus
}
#endif

#endif // SOLDERING_IRON_HAL_H
