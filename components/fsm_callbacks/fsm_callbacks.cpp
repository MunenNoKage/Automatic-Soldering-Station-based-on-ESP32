/**
 * @file fsm_callbacks.cpp
 * @brief Implementation of FSM state callback functions
 *
 * This module implements all FSM state callbacks in a separate file
 * to improve code organization and maintainability.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2026
 */

#include "fsm_callbacks.h"
#include <cmath>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Forward declarations of callback functions
extern "C" {
    bool on_enter_idle(void* user_data);
    bool on_enter_manual_control(void* user_data);
    bool on_enter_manual_executing(void* user_data);
    bool on_enter_calibration(void* user_data);
    bool on_enter_ready(void* user_data);
    bool on_enter_heating(void* user_data);
    bool on_enter_executing(void* user_data);
    bool on_enter_paused(void* user_data);
    bool on_enter_normal_exit(void* user_data);
    bool on_enter_error_state(void* user_data);
    bool on_enter_lock_state(void* user_data);

    bool on_exit_manual_control(void* user_data);

    bool on_execute_manual_executing(void* user_data);
    bool on_execute_calibration(void* user_data);
    bool on_execute_heating(void* user_data);
    bool on_execute_executing(void* user_data);
    bool on_execute_normal_exit(void* user_data);
}

namespace {
    static const char *TAG = "FSM_CALLBACKS";

    // Hardware references (initialized via fsm_callbacks_init)
    StepperMotor* motor_x = nullptr;
    StepperMotor* motor_y = nullptr;
    StepperMotor* motor_z = nullptr;
    StepperMotor* motor_s = nullptr;
    soldering_iron_handle_t iron_handle = nullptr;
    temperature_sensor_handle_t temp_sensor_handle = nullptr;
    execution_sub_fsm_t* exec_sub_fsm_ptr = nullptr;
    char** g_gcode_buffer_ptr = nullptr;
    size_t* g_gcode_size_ptr = nullptr;
    bool* g_gcode_loaded_ptr = nullptr;
    fsm_controller_handle_t fsm_handle = nullptr;

    /**
     * @brief Read current temperature from sensor
     * @return Temperature in Celsius, or -1.0 on error
     */
    double get_current_temperature() {
        double temp = 0.0;
        if (!temp_sensor_handle) {
            return -1.0;
        }

        esp_err_t ret = temperature_sensor_hal_read_temperature(temp_sensor_handle, &temp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read temperature");
            return -1.0;
        }

        return temp;
    }
} // anonymous namespace

extern "C" {

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
) {
    motor_x = x_motor;
    motor_y = y_motor;
    motor_z = z_motor;
    motor_s = s_motor;
    iron_handle = iron;
    temp_sensor_handle = temp_sensor;
    exec_sub_fsm_ptr = exec_fsm;
    g_gcode_buffer_ptr = gcode_buffer;
    g_gcode_size_ptr = gcode_size;
    g_gcode_loaded_ptr = gcode_loaded;
    fsm_handle = fsm;

    ESP_LOGI(TAG, "FSM callbacks module initialized");
}

void fsm_callbacks_register_all(fsm_controller_handle_t fsm) {
    // Register enter callbacks
    fsm_controller_register_enter_callback(fsm, FSM_STATE_IDLE, on_enter_idle, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_MANUAL_CONTROL, on_enter_manual_control, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_MANUAL_EXECUTING, on_enter_manual_executing, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_CALIBRATION, on_enter_calibration, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_READY, on_enter_ready, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_HEATING, on_enter_heating, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_EXECUTING, on_enter_executing, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_PAUSED, on_enter_paused, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_NORMAL_EXIT, on_enter_normal_exit, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_CALIBRATION_ERROR, on_enter_error_state, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_HEATING_ERROR, on_enter_error_state, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_DATA_ERROR, on_enter_error_state, nullptr);
    fsm_controller_register_enter_callback(fsm, FSM_STATE_LOCK, on_enter_lock_state, nullptr);

    // Register exit callbacks
    fsm_controller_register_exit_callback(fsm, FSM_STATE_MANUAL_CONTROL, on_exit_manual_control, nullptr);

    // Register execute callbacks
    fsm_controller_register_execute_callback(fsm, FSM_STATE_MANUAL_EXECUTING, on_execute_manual_executing, nullptr);
    fsm_controller_register_execute_callback(fsm, FSM_STATE_CALIBRATION, on_execute_calibration, nullptr);
    fsm_controller_register_execute_callback(fsm, FSM_STATE_HEATING, on_execute_heating, nullptr);
    fsm_controller_register_execute_callback(fsm, FSM_STATE_EXECUTING, on_execute_executing, nullptr);
    fsm_controller_register_execute_callback(fsm, FSM_STATE_NORMAL_EXIT, on_execute_normal_exit, nullptr);

    ESP_LOGI(TAG, "All FSM callbacks registered");
}

// ============================================================================
// State Enter Callbacks
// ============================================================================

bool on_enter_idle(void* user_data) {
    ESP_LOGI(TAG, "FSM: IDLE - System ready");

    // Ensure heater is off when idle
    if (iron_handle) {
        soldering_iron_hal_set_enable(iron_handle, false);
    }

    return true;
}

bool on_enter_manual_control(void* user_data) {
    ESP_LOGI(TAG, "FSM: MANUAL_CONTROL - Ready for manual commands");
    return true;
}

bool on_enter_manual_executing(void* user_data) {
    ESP_LOGI(TAG, "FSM: MANUAL_EXECUTING - Executing manual movement");

    // Enable motors
    motor_x->setEnable(true);
    motor_y->setEnable(true);
    motor_z->setEnable(true);
    motor_s->setEnable(true);

    return true;
}

bool on_enter_calibration(void* user_data) {
    ESP_LOGI(TAG, "FSM: CALIBRATION");
    return true;
}

bool on_enter_ready(void* user_data) {
    ESP_LOGI(TAG, "FSM: READY - Task approved, awaiting start");
    return true;
}

bool on_enter_heating(void* user_data) {
    ESP_LOGI(TAG, "FSM: HEATING - Starting temperature control");

    if (!iron_handle) {
        ESP_LOGE(TAG, "Soldering iron not initialized!");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_HEATING_ERROR);
        return false;
    }

    // Get configuration from FSM
    const fsm_config_t* config = fsm_controller_get_config(fsm_handle);
    if (!config) {
        ESP_LOGE(TAG, "Failed to get FSM configuration!");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_HEATING_ERROR);
        return false;
    }

    // Set target temperature
    soldering_iron_hal_set_target_temperature(iron_handle, config->target_temperature);
    ESP_LOGI(TAG, "Target temperature: %.1f°C", config->target_temperature);

    // Enable heater
    soldering_iron_hal_set_enable(iron_handle, true);
    ESP_LOGI(TAG, "Heater enabled");

    return true;
}

bool on_enter_executing(void* user_data) {
    motor_x->setEnable(true);
    motor_y->setEnable(true);
    motor_z->setEnable(true);
    motor_s->setEnable(true);

    // Preserve origin coordinates before reinitializing
    double saved_x_origin = 0.0;
    double saved_y_origin = 0.0;
    exec_sub_fsm_get_origin(exec_sub_fsm_ptr, &saved_x_origin, &saved_y_origin);

    execution_config_t exec_config = {
        .safe_z_height = motor_z->mm_to_microsteps(140),       // 140mm in steps
        .soldering_z_height = motor_z->mm_to_microsteps(160),  // 160mm in steps
        .home_x = 0,
        .home_y = 0,
        .home_z = 0,
        .x_origin = saved_x_origin,
        .y_origin = saved_y_origin
    };

    exec_sub_fsm_init(exec_sub_fsm_ptr, &exec_config);

    // Check if GCode is loaded in RAM
    if (!(*g_gcode_loaded_ptr) || !(*g_gcode_buffer_ptr)) {
        ESP_LOGE(TAG, "=== NO GCODE UPLOADED ===");
        ESP_LOGE(TAG, "Cannot execute - no GCode in RAM");
        ESP_LOGE(TAG, "Please upload GCode via POST /api/gcode/upload");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    ESP_LOGI(TAG, "=== EXECUTING FROM GCODE ===");
    ESP_LOGI(TAG, "GCode buffer: %d bytes in RAM", *g_gcode_size_ptr);

    if (!exec_sub_fsm_load_gcode_from_ram(exec_sub_fsm_ptr, *g_gcode_buffer_ptr, *g_gcode_size_ptr)) {
        ESP_LOGE(TAG, "Failed to load GCode from RAM");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    ESP_LOGI(TAG, "GCode parser initialized - starting execution");
    return true;
}

bool on_enter_paused(void* user_data) {
    ESP_LOGI(TAG, "FSM: PAUSED - Task paused, awaiting resume or exit command");

    // Keep heater active during pause to maintain temperature
    // Motors remain in their current positions

    return true;
}

bool on_enter_error_state(void* user_data) {
    ESP_LOGE(TAG, "FSM: ERROR STATE - System entered error state");

    // Disable heater immediately for safety
    if (iron_handle) {
        soldering_iron_hal_set_enable(iron_handle, false);
        ESP_LOGI(TAG, "Heater disabled due to error");
    }

    // Disable all motors
    if (motor_x) motor_x->setEnable(false);
    if (motor_y) motor_y->setEnable(false);
    if (motor_z) motor_z->setEnable(false);
    if (motor_s) motor_s->setEnable(false);

    return true;
}

bool on_enter_lock_state(void* user_data) {
    ESP_LOGE(TAG, "FSM: LOCK STATE - System locked, manual restart required");

    // Ensure everything is off
    if (iron_handle) {
        soldering_iron_hal_set_enable(iron_handle, false);
    }

    if (motor_x) motor_x->setEnable(false);
    if (motor_y) motor_y->setEnable(false);
    if (motor_z) motor_z->setEnable(false);
    if (motor_s) motor_s->setEnable(false);

    ESP_LOGE(TAG, "System requires manual reset/restart");

    return true;
}

bool on_enter_normal_exit(void* user_data) {
    ESP_LOGI(TAG, "FSM: NORMAL_EXIT - Returning to home and starting cooldown");

    // Disable heater immediately
    if (iron_handle) {
        soldering_iron_hal_set_enable(iron_handle, false);
        ESP_LOGI(TAG, "Heater disabled - Starting cooldown");
    }

    // Return all axes to home position (0, 0, 0) before disabling motors
    ESP_LOGI(TAG, "Returning to home position (0, 0, 0)");
    motor_x->setTargetPosition(0);
    motor_y->setTargetPosition(0);
    motor_z->setTargetPosition(0);

    // Move all axes to home
    uint32_t x_steps = static_cast<uint32_t>(std::abs(motor_x->getPosition()));
    uint32_t y_steps = static_cast<uint32_t>(std::abs(motor_y->getPosition()));
    uint32_t z_steps = static_cast<uint32_t>(std::abs(motor_z->getPosition()));

    if (x_steps > 0) motor_x->stepMultipleToTarget(x_steps);
    if (y_steps > 0) motor_y->stepMultipleToTarget(y_steps);
    if (z_steps > 0) motor_z->stepMultipleToTarget(z_steps);

    ESP_LOGI(TAG, "Home position reached - Motors at (0, 0, 0)");

    // Now disable motors after reaching home
    motor_x->setEnable(false);
    motor_y->setEnable(false);
    motor_z->setEnable(false);
    motor_s->setEnable(false);

    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (ctx) {
        fsm_execution_context_init(ctx);
        ctx->operation_complete = false;
    }

    return true;
}

// ============================================================================
// State Exit Callbacks
// ============================================================================

bool on_exit_manual_control(void* user_data) {
    ESP_LOGI(TAG, "FSM: Exiting MANUAL_CONTROL - Disabling solder motor");

    // Disable solder motor when exiting manual mode
    motor_s->setEnable(false);

    return true;
}

// ============================================================================
// State Execute Callbacks
// ============================================================================

bool on_execute_manual_executing(void* user_data) {
    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (!ctx) return false;

    // Only execute once per entry to this state
    if (ctx->operation_complete) {
        return true;
    }

    // Check if GCode command is available
    if (!(*g_gcode_loaded_ptr) || !(*g_gcode_buffer_ptr)) {
        ESP_LOGE(TAG, "No manual G-code command available");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    ESP_LOGI(TAG, "Executing manual command: %s", *g_gcode_buffer_ptr);

    // Preserve origin coordinates before reinitializing
    double saved_x_origin = 0.0;
    double saved_y_origin = 0.0;
    exec_sub_fsm_get_origin(exec_sub_fsm_ptr, &saved_x_origin, &saved_y_origin);

    // Initialize execution FSM for manual command
    execution_config_t exec_config = {
        .safe_z_height = motor_z->mm_to_microsteps(140),
        .soldering_z_height = motor_z->mm_to_microsteps(160),
        .home_x = 0,
        .home_y = 0,
        .home_z = 0,
        .x_origin = saved_x_origin,
        .y_origin = saved_y_origin
    };
    exec_sub_fsm_init(exec_sub_fsm_ptr, &exec_config);

    // Load and parse the single G-code command
    if (!exec_sub_fsm_load_gcode_from_ram(exec_sub_fsm_ptr, *g_gcode_buffer_ptr, *g_gcode_size_ptr)) {
        ESP_LOGE(TAG, "Failed to parse manual G-code command");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    // Execute the command using manual mode (no automatic Z management)
    exec_sub_fsm_process_gcode_manual(exec_sub_fsm_ptr);

    // Cleanup parser
    exec_sub_fsm_cleanup_gcode(exec_sub_fsm_ptr);

    ESP_LOGI(TAG, "Manual movement complete");
    ctx->operation_complete = true;

    // Post event to return to MANUAL_CONTROL state
    fsm_controller_post_event(fsm_handle, FSM_EVENT_MANUAL_MOVE_DONE);

    return true;
}

bool on_execute_calibration(void* user_data) {
    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (!ctx) return false;

    if (ctx->iteration_count == 0) {
        ESP_LOGI(TAG, "Calibrating X-axis");
        motor_x->calibrate();
        motor_x->setEnable(false);
        ctx->iteration_count = 1;
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (ctx->iteration_count == 1) {
        ESP_LOGI(TAG, "Calibrating Y-axis");
        motor_y->calibrate();
        motor_y->setEnable(false);
        ctx->iteration_count = 2;
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (ctx->iteration_count == 2) {
        ESP_LOGI(TAG, "Calibrating Z-axis");
        motor_z->calibrate();
        motor_z->setEnable(false);
        ctx->iteration_count = 3;
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (ctx->iteration_count == 3 && !ctx->operation_complete) {
        uint32_t time_since_start = (esp_timer_get_time() / 1000) - ctx->start_time_ms;
        if (time_since_start > 500) {
            ESP_LOGI(TAG, "Calibration complete");
            ctx->operation_complete = true;

            // Post appropriate event based on mode
            if (ctx->is_manual_mode) {
                ESP_LOGI(TAG, "Calibration for manual mode - transitioning to MANUAL_CONTROL");
                fsm_controller_post_event(fsm_handle, FSM_EVENT_CALIBRATION_DONE);
            } else {
                ESP_LOGI(TAG, "Calibration for automatic mode - transitioning to READY");
                fsm_controller_post_event(fsm_handle, FSM_EVENT_CALIBRATION_SUCCESS);
            }
        }
    }

    return true;
}

bool on_execute_heating(void* user_data) {
    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (!ctx) return false;

    // Get configuration from FSM
    const fsm_config_t* config = fsm_controller_get_config(fsm_handle);
    if (!config) return false;

    // MAX6675 requires minimum 220ms between readings for new conversion
    // Only read temperature every 250ms to ensure fresh data
    static uint32_t last_temp_read_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;

    static double current_temp = 0.0;

    if (current_time - last_temp_read_time >= 250) {
        // Read current temperature
        current_temp = get_current_temperature();
        ESP_LOGI(TAG, "temp: %.2f", current_temp);
        if (current_temp < 0) {
            ESP_LOGE(TAG, "Temperature sensor error: %.2f", current_temp);
            fsm_controller_post_event(fsm_handle, FSM_EVENT_HEATING_ERROR);
            return false;
        }
        last_temp_read_time = current_time;
    }

    // Update PID controller with current temperature
    soldering_iron_hal_update_control(iron_handle, current_temp);

    // Get target temperature
    double target_temp = soldering_iron_hal_get_target_temperature(iron_handle);

    // Check if target temperature reached
    double temp_diff = fabs(current_temp - target_temp);

    // Log temperature every 2 seconds
    uint32_t time_heating = (esp_timer_get_time() / 1000) - ctx->start_time_ms;
    if (time_heating % 2000 < 100) {  // Every ~2 seconds
        double power = soldering_iron_hal_get_power(iron_handle);
        ESP_LOGI(TAG, "Heating: Current=%.1f°C, Target=%.1f°C, Diff=%.1f°C, Power=%.1f%%",
                 current_temp, target_temp, temp_diff, power);
    }

    // Check for timeout
    if (time_heating > config->heating_timeout_ms) {
        ESP_LOGE(TAG, "Heating timeout!");
        soldering_iron_hal_set_enable(iron_handle, false);
        fsm_controller_post_event(fsm_handle, FSM_EVENT_HEATING_ERROR);
        return false;
    }

    // Temperature reached and stable
    if (temp_diff <= config->temperature_tolerance && !ctx->operation_complete) {
        ESP_LOGI(TAG, "Target temperature reached: %.1f°C (±%.1f°C)", current_temp, config->temperature_tolerance);
        ctx->operation_complete = true;
        fsm_controller_post_event(fsm_handle, FSM_EVENT_HEATING_SUCCESS);
    }

    return true;
}

bool on_execute_executing(void* user_data) {
    // Maintain temperature during execution
    double current_temp = get_current_temperature();
    if (current_temp > 0 && iron_handle) {
        soldering_iron_hal_update_control(iron_handle, current_temp);

        // Check for temperature errors during execution
        double target_temp = soldering_iron_hal_get_target_temperature(iron_handle);
        if (fabs(current_temp - target_temp) > 30.0) {  // Temperature drift > 30°C
            ESP_LOGW(TAG, "Temperature drift detected: %.1f°C (target: %.1f°C)",
                     current_temp, target_temp);
        }
    }

    // Execute GCode line by line
    exec_sub_fsm_process_gcode(exec_sub_fsm_ptr);

    if (exec_sub_fsm_get_state(exec_sub_fsm_ptr) == EXEC_STATE_COMPLETE) {
        ESP_LOGI(TAG, "GCode execution complete: %d commands executed",
                 exec_sub_fsm_get_completed_count(exec_sub_fsm_ptr));

        // Cleanup GCode resources
        exec_sub_fsm_cleanup_gcode(exec_sub_fsm_ptr);

        fsm_controller_post_event(fsm_handle, FSM_EVENT_TASK_DONE);
    }

    return true;
}

bool on_execute_normal_exit(void* user_data) {
    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (!ctx) return false;

    // Get configuration from FSM
    const fsm_config_t* config = fsm_controller_get_config(fsm_handle);
    if (!config) return false;

    // MAX6675 requires minimum 220ms between readings for new conversion
    // Only read temperature every 250ms to ensure fresh data
    static uint32_t last_temp_read_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;

    static double current_temp = 200.0;  // Default to hot temperature

    if (current_time - last_temp_read_time >= 250) {
        // Read current temperature
        double temp = get_current_temperature();
        if (temp < 0) {
            ESP_LOGW(TAG, "Cannot read temperature during cooldown");
            // Keep previous temperature value
        } else {
            current_temp = temp;
        }
        last_temp_read_time = current_time;
    }

    uint32_t time_cooldown = (esp_timer_get_time() / 1000) - ctx->start_time_ms;

    // Log temperature every 5 seconds during cooldown
    if (time_cooldown % 5000 < 100) {
        ESP_LOGI(TAG, "Cooldown: Current=%.1f°C, Safe=%.1f°C, Time=%lus",
                 current_temp, config->safe_temperature, time_cooldown / 1000);
    }

    // Check for timeout (10 minutes)
    if (time_cooldown > config->cooldown_timeout_ms) {
        ESP_LOGW(TAG, "Cooldown timeout (10 min)! Current temp: %.1f°C", current_temp);
        fsm_controller_post_event(fsm_handle, FSM_EVENT_COOLING_ERROR);
        return false;
    }

    // Check if cooled down to safe temperature
    if (current_temp <= config->safe_temperature && !ctx->operation_complete) {
        ESP_LOGI(TAG, "Cooldown complete - System safe at %.1f°C", current_temp);
        ctx->operation_complete = true;
        fsm_controller_post_event(fsm_handle, FSM_EVENT_COOLDOWN_COMPLETE);
    }

    return true;
}

} // extern "C"
