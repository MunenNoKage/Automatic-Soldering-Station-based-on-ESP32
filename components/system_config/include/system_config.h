/**
 * @file system_config.h
 * @brief Centralized system configuration and initialization
 *
 * This module provides a single place for all system configurations
 * and initialization functions for the Automatic Soldering Station.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2026
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "StepperMotor.hpp"
#include "soldering_iron_hal.h"
#include "temperature_sensor_hal.h"
#include "fsm_controller.h"
#include "execution_fsm.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Motor Configuration Parameters
// ============================================================================

/**
 * @brief Motor axis configuration structure
 */
typedef struct {
    // Movement parameters
    double k_slope;                    // Acceleration slope factor
    int32_t minimal_step_delay_us;     // Minimum delay between steps (microseconds)
    uint32_t microsteps_in_mm;         // Microsteps per millimeter
    stepper_direction_t default_dir;   // Default direction
} motor_axis_config_t;

/**
 * @brief Execution FSM configuration parameters
 */
typedef struct {
    double safe_z_height_mm;           // Safe Z height in millimeters
    double soldering_z_height_mm;      // Soldering Z height in millimeters
    int32_t home_x;                    // Home position X
    int32_t home_y;                    // Home position Y
    int32_t home_z;                    // Home position Z
} execution_fsm_config_t;

/**
 * @brief PID controller configuration
 */
typedef struct {
    double kp;                         // Proportional gain
    double ki;                         // Integral gain
    double kd;                         // Derivative gain
} pid_config_t;

/**
 * @brief Complete system configuration
 */
typedef struct {
    // Motor configurations
    motor_axis_config_t motor_x_config;
    motor_axis_config_t motor_y_config;
    motor_axis_config_t motor_z_config;
    motor_axis_config_t motor_s_config;

    // Execution FSM configuration
    execution_fsm_config_t exec_fsm_config;

    // Temperature control
    pid_config_t pid_config;
    double min_temperature;
    double max_temperature;

    // FSM timing and thresholds
    uint32_t tick_rate_ms;
    float target_temperature;
    float temperature_tolerance;
    uint32_t heating_timeout_ms;
    uint32_t calibration_timeout_ms;
    float safe_temperature;
    uint32_t cooldown_timeout_ms;

    // WiFi configuration
    const char* wifi_ssid;
    uint8_t wifi_channel;
    uint8_t wifi_max_connections;

    // Web server configuration
    uint16_t web_port;
    uint8_t web_max_uri_handlers;
    uint8_t web_max_resp_headers;
    bool web_enable_websocket;
} system_config_t;

// ============================================================================
// Global Hardware Handles (accessible by other modules)
// ============================================================================

extern StepperMotor* motor_x;
extern StepperMotor* motor_y;
extern StepperMotor* motor_z;
extern StepperMotor* motor_s;

extern soldering_iron_handle_t g_iron_handle;
extern temperature_sensor_handle_t g_temp_sensor_handle;
extern fsm_controller_handle_t g_fsm_handle;

extern char* g_gcode_buffer;
extern size_t g_gcode_size;
extern bool g_gcode_loaded;
extern SemaphoreHandle_t g_gcode_mutex;

extern execution_sub_fsm_t g_exec_sub_fsm;

// ============================================================================
// System Configuration Functions
// ============================================================================

/**
 * @brief Get the default system configuration
 *
 * @return Pointer to default configuration structure
 */
const system_config_t* system_config_get_default(void);

/**
 * @brief Get the current active system configuration
 *
 * @return Pointer to active configuration structure
 */
const system_config_t* system_config_get_active(void);

/**
 * @brief Update system configuration parameters
 *
 * @param config New configuration to apply
 * @return true if configuration was applied successfully
 */
bool system_config_set(const system_config_t* config);

// ============================================================================
// System Initialization Functions
// ============================================================================

/**
 * @brief Initialize all stepper motors with current configuration
 *
 * @return true if all motors initialized successfully
 */
bool system_init_motors(void);

/**
 * @brief Initialize heating system (soldering iron and temperature sensor)
 *
 * @return true if heating system initialized successfully
 */
bool system_init_heating(void);

/**
 * @brief Initialize FSM controller with current configuration
 *
 * @return true if FSM initialized successfully
 */
bool system_init_fsm(void);

/**
 * @brief Initialize WiFi and web server
 *
 * @return true if network initialized successfully
 */
bool system_init_network(void);

/**
 * @brief Initialize all system components in correct order
 *
 * This function initializes:
 * - GCode buffer mutex
 * - Motors
 * - Heating system
 * - FSM controller
 * - Network (WiFi + Web server)
 *
 * @return true if all subsystems initialized successfully
 */
bool system_init_all(void);

/**
 * @brief Start FSM processing task
 *
 * @return true if task created successfully
 */
bool system_start_fsm_task(void);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_CONFIG_H
