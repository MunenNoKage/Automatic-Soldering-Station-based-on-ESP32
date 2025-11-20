/**
 * @file stepper_motor_hal.h
 * @brief Hardware Abstraction Layer for TMC2208 stepper motor driver
 * 
 * Provides low-level C interface for controlling stepper motors via TMC2208 driver.
 * Supports microstepping modes (1/4, 1/8, 1/16) via MS pins for precision control.
 * Note: 1/32 microstepping requires UART configuration (not supported via pins).
 * Used for X, Y, Z, and solder supply (S) axis control.
 */

#ifndef STEPPER_MOTOR_HAL_H
#define STEPPER_MOTOR_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Motor rotation direction
 */
typedef enum {
    STEPPER_DIR_CLOCKWISE = 0,
    STEPPER_DIR_COUNTERCLOCKWISE = 1,
} stepper_direction_t;

/**
 * @brief Motor enable/disable state
 */
typedef enum {
    STEPPER_ENABLE = 0,
    STEPPER_DISABLE = 1,
} stepper_enable_t;

/**
 * @brief Stepper motor configuration structure
 * Represent general state of motor
 */
typedef struct {
    gpio_num_t step_pin;
    gpio_num_t dir_pin;
    gpio_num_t enable_pin;
    gpio_num_t endpoint_pin;    // Limit switch input pin
} stepper_motor_config_t;

/**
 * @brief Stepper motor handle
 * Represent specific motor instance current state
 */
typedef struct stepper_motor_handle_s* stepper_motor_handle_t;

/**
 * @brief Initialize stepper motor driver
 * 
 * @param config Pointer to motor configuration structure
 * 
 * @return stepper_motor_handle_t Handle to the initialized motor instance, or NULL on failure
 * @note After initialization:
 * 
 * - Motor is disabled.
 *
 * - Direction is set to clockwise.
 *
 * - Microstepping mode is set to full step.
 * 
 * - Position counter is set to zero.
 */
stepper_motor_handle_t stepper_motor_hal_init(const stepper_motor_config_t* config);

/**
 * @brief Deinitialize stepper motor driver
 * 
 * @param handle Handle to the motor instance
 * 
 * @note After deinitialization:
 * 
 * - Motor is disabled.
 * 
 * - GPIO pins configuration remain unchanged.
 * 
 * - Memory allocated for the handle is freed.
 */
void stepper_motor_hal_deinit(stepper_motor_handle_t handle);

/**
 * @brief Enable or disable the motor
 */
void stepper_motor_hal_set_enable(stepper_motor_handle_t handle, bool enable);

/**
 * @brief Set motor direction
 */
void stepper_motor_hal_set_direction(stepper_motor_handle_t handle, stepper_direction_t direction);

/**
 * @brief Execute single step
 */
void stepper_motor_hal_step(stepper_motor_handle_t handle, uint32_t signal_width_us);

/**
 * @brief Execute multiple steps
 */
void stepper_motor_hal_step_multiple(stepper_motor_handle_t handle, uint32_t steps);

/**
 * @brief Set time between steps in microseconds
 * 
 * @param handle Handle to the motor instance
 * @param step_time_us Time between steps in microseconds (minimum 100us)
 * 
 * @note This affects the speed when using stepper_motor_hal_step_multiple()
 */
void stepper_motor_hal_set_step_time(stepper_motor_handle_t handle, uint32_t step_time_us);

/**
 * @brief Get current step time in microseconds
 * 
 * @param handle Handle to the motor instance
 * @return uint32_t Current step time in microseconds, or 0 on error
 */
uint32_t stepper_motor_hal_get_step_time(stepper_motor_handle_t handle);

/**
 * @brief Get current motor direction
 * 
 * @param handle Handle to the motor instance
 * @return stepper_direction_t Current direction
 */
stepper_direction_t stepper_motor_hal_get_direction(stepper_motor_handle_t handle);

/**
 * @brief Check if the motor's endpoint switch is triggered
 * 
 * @param handle Handle to the motor instance
 * @return bool true if endpoint switch is triggered (active LOW), false otherwise or if no endpoint configured
 * @note Assumes endpoint switch is connected between GPIO and GND (active LOW)
 */
bool stepper_motor_hal_endpoint_reached(stepper_motor_handle_t handle);

void reset_watchdog_timer();

#ifdef __cplusplus
}
#endif

#endif // STEPPER_MOTOR_HAL_H
