/**
 * @file fsm_callbacks.h
 * @brief FSM state callback functions for the Automatic Soldering Station
 *
 * This module contains all FSM state enter/exit/execute callbacks organized
 * in a separate namespace for better code organization and maintainability.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2026
 */

#ifndef FSM_CALLBACKS_H
#define FSM_CALLBACKS_H

#include "fsm_controller.h"
#include "StepperMotor.hpp"
#include "soldering_iron_hal.h"
#include "temperature_sensor_hal.h"
#include "execution_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the FSM callbacks module with required hardware references
 *
 * @param x_motor Pointer to X-axis stepper motor
 * @param y_motor Pointer to Y-axis stepper motor
 * @param z_motor Pointer to Z-axis stepper motor
 * @param s_motor Pointer to solder supply stepper motor
 * @param iron Soldering iron handle
 * @param temp_sensor Temperature sensor handle
 * @param exec_fsm Pointer to execution sub-FSM instance
 * @param gcode_buffer Pointer to GCode buffer pointer
 * @param gcode_size Pointer to GCode buffer size variable
 * @param gcode_loaded Pointer to GCode loaded flag
 * @param fsm FSM controller handle
 */
void fsm_callbacks_init(
    StepperMotor* x_motor,
    StepperMotor* y_motor,
    StepperMotor* z_motor,
    StepperMotor* s_motor,
    soldering_iron_handle_t iron,
    temperature_sensor_handle_t temp_sensor,
    execution_sub_fsm_t* exec_fsm,
    char** gcode_buffer,
    size_t* gcode_size,
    bool* gcode_loaded,
    fsm_controller_handle_t fsm
);

/**
 * @brief Register all FSM callbacks with the FSM controller
 *
 * @param fsm FSM controller handle to register callbacks with
 */
void fsm_callbacks_register_all(fsm_controller_handle_t fsm);

// ============================================================================
// State Enter Callbacks
// ============================================================================

bool on_enter_idle(void* user_data);
bool on_enter_manual_control(void* user_data);
bool on_enter_manual_executing(void* user_data);
bool on_enter_calibration(void* user_data);
bool on_enter_ready(void* user_data);
bool on_enter_heating(void* user_data);
bool on_enter_executing(void* user_data);
bool on_enter_normal_exit(void* user_data);

// ============================================================================
// State Exit Callbacks
// ============================================================================

bool on_exit_manual_control(void* user_data);

// ============================================================================
// State Execute Callbacks
// ============================================================================

bool on_execute_manual_executing(void* user_data);
bool on_execute_calibration(void* user_data);
bool on_execute_heating(void* user_data);
bool on_execute_executing(void* user_data);
bool on_execute_normal_exit(void* user_data);

#ifdef __cplusplus
}
#endif

#endif // FSM_CALLBACKS_H
