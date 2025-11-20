/**
 * @file main.cpp
 * @brief Simple FSM-based motor control application
 *
 * Uses FSM controller to manage motor movements through different states.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2025
 */

#include <stdio.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "fsm_controller.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "stepper_motor_hal.h"
#include "StepperMotor.hpp"
#include "execution_fsm.h"
#include "soldering_iron_hal.h"
#include "temperature_sensor_hal.h"

static const char *TAG = "MAIN";

// Global motor instances (exported for execution_fsm)
StepperMotor* motor_x = nullptr;
StepperMotor* motor_y = nullptr;
StepperMotor* motor_z = nullptr;
StepperMotor* motor_s = nullptr;

// Soldering iron and temperature sensor handles
static soldering_iron_handle_t iron_handle = nullptr;
static temperature_sensor_handle_t temp_sensor_handle = nullptr;

// FSM controller handle
static fsm_controller_handle_t fsm_handle = nullptr;

// Global GCode buffer (RAM storage instead of filesystem)
char* g_gcode_buffer = nullptr;
size_t g_gcode_size = 0;
bool g_gcode_loaded = false;
SemaphoreHandle_t g_gcode_mutex = nullptr;

// Execution sub-FSM instance (initialized in on_enter_executing)
static execution_sub_fsm_t exec_sub_fsm;

/**
 * @brief Initialize all stepper motors
 */
static void init_motors() {
    ESP_LOGI(TAG, "Initializing stepper motors...");

    // X-Axis Motor Configuration
    stepper_motor_config_t config_x = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_MIN_ENDPOINT_PIN)  // Changed from ENDPOINT to MIN_ENDPOINT
    };
    motor_x = new StepperMotor(config_x, CONFIG_MOTOR_X_MICROSTEPS_IN_MM, STEPPER_DIR_COUNTERCLOCKWISE);
    if (!motor_x->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize X-axis motor");
        return;
    }
    ESP_LOGI(TAG, "X-axis motor initialized");

    // Y-Axis Motor Configuration
    stepper_motor_config_t config_y = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_MIN_ENDPOINT_PIN)  // Changed from ENDPOINT to MIN_ENDPOINT
    };
    motor_y = new StepperMotor(config_y, CONFIG_MOTOR_Y_MICROSTEPS_IN_MM, STEPPER_DIR_CLOCKWISE);
    if (!motor_y->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize Y-axis motor");
        return;
    }
    ESP_LOGI(TAG, "Y-axis motor initialized");

    // Z-Axis Motor Configuration
    stepper_motor_config_t config_z = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_MIN_ENDPOINT_PIN)  // Changed from ENDPOINT to MIN_ENDPOINT
    };
    motor_z = new StepperMotor(config_z, CONFIG_MOTOR_Z_MICROSTEPS_IN_MM, STEPPER_DIR_CLOCKWISE);
    if (!motor_z->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize Z-axis motor");
        return;
    }
    ESP_LOGI(TAG, "Z-axis motor initialized");

    // Solder Supply Motor Configuration
    stepper_motor_config_t config_s = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_ENABLE_PIN),
        .endpoint_pin = GPIO_NUM_NC  // No endpoint switch for solder supply
    };
    motor_s = new StepperMotor(config_s, CONFIG_MOTOR_S_MICROSTEPS_IN_MM, STEPPER_DIR_CLOCKWISE);
    if (!motor_s->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize solder supply motor");
        return;
    }
    ESP_LOGI(TAG, "Solder supply motor initialized");
}

/**
 * @brief Initialize soldering iron and temperature sensor
 */
static void init_heating_system() {
    ESP_LOGI(TAG, "Initializing heating system...");

    // Initialize temperature sensor (MAX6675)
    temperature_sensor_config_t temp_config = {
        .host_id = VSPI_HOST,
        .pin_miso = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_MISO_PIN),
        .pin_mosi = GPIO_NUM_NC,  // MAX6675 is read-only
        .pin_clk = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_CLK_PIN),
        .pin_cs = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_CS_PIN),
        .dma_chan = 0,
        .clock_speed_hz = 2000000  // 2 MHz for MAX6675
    };

    temp_sensor_handle = temperature_sensor_hal_init(&temp_config);
    if (!temp_sensor_handle) {
        ESP_LOGE(TAG, "Failed to initialize temperature sensor");
        return;
    }
    ESP_LOGI(TAG, "Temperature sensor initialized");

    // Initialize soldering iron PWM control
    soldering_iron_config_t iron_config = {
        .heater_pwm_pin = static_cast<gpio_num_t>(CONFIG_SOLDERING_IRON_PWM_PIN),
        .pwm_timer = LEDC_TIMER_0,
        .pwm_channel = LEDC_CHANNEL_0,
        .pwm_frequency = CONFIG_SOLDERING_IRON_PWM_FREQUENCY,
        .pwm_resolution = LEDC_TIMER_10_BIT,
        .max_temperature = static_cast<double>(CONFIG_SOLDERING_IRON_MAX_TEMP),
        .min_temperature = 20.0
    };

    iron_handle = soldering_iron_hal_init(&iron_config);
    if (!iron_handle) {
        ESP_LOGE(TAG, "Failed to initialize soldering iron");
        return;
    }

    // Set PID constants for temperature control
    // Increased Kp for faster response, reduced Ki and Kd to prevent oscillation
    soldering_iron_hal_set_pid_constants(iron_handle, 10.0, 0.1, 0.5);
    ESP_LOGI(TAG, "Soldering iron initialized with PID control (Kp=10.0, Ki=0.1, Kd=0.5)");
}

/**
 * @brief Read current temperature from sensor
 * @return Temperature in Celsius, or -1.0 on error
 */
static double get_current_temperature() {
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

static bool on_enter_idle(void* user_data) {
    ESP_LOGI(TAG, "FSM: IDLE - System ready");

    // Ensure heater is off when idle
    if (iron_handle) {
        soldering_iron_hal_set_enable(iron_handle, false);
    }

    return true;
}

static bool on_enter_calibration(void* user_data) {
    ESP_LOGI(TAG, "FSM: CALIBRATION");
    return true;
}

static bool on_execute_calibration(void* user_data) {
    fsm_execution_context_t* ctx = fsm_controller_get_execution_context(fsm_handle);
    if (!ctx) return false;

    if (ctx->iteration_count == 0) {
        ESP_LOGI(TAG, "Calibrating X-axis");
        motor_x->calibrate();
        ctx->iteration_count = 1;
    } else if (ctx->iteration_count == 1) {
        ESP_LOGI(TAG, "Calibrating Y-axis");
        motor_y->calibrate();
        ctx->iteration_count = 2;
    } else if (ctx->iteration_count == 2) {
        ESP_LOGI(TAG, "Calibrating Z-axis");
        motor_z->calibrate();
        ctx->iteration_count = 3;
    } else if (ctx->iteration_count == 3 && !ctx->operation_complete) {
        uint32_t time_since_start = (esp_timer_get_time() / 1000) - ctx->start_time_ms;
        if (time_since_start > 500) {
            ESP_LOGI(TAG, "Calibration complete");
            ctx->operation_complete = true;
            fsm_controller_post_event(fsm_handle, FSM_EVENT_CALIBRATION_SUCCESS);
        }
    }

    return true;
}

static bool on_enter_ready(void* user_data) {
    ESP_LOGI(TAG, "FSM: READY - Task approved, awaiting start");
    return true;
}

static bool on_enter_heating(void* user_data) {
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

static bool on_execute_heating(void* user_data) {
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
        if (current_temp < 0) {
            ESP_LOGE(TAG, "Temperature sensor error");
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

static bool on_enter_executing(void* user_data) {
    motor_x->setEnable(true);
    motor_y->setEnable(true);
    motor_z->setEnable(true);
    motor_s->setEnable(true);

    execution_config_t exec_config = {
        .safe_z_height = motor_z->mm_to_microsteps(140),       // 140mm in steps
        .soldering_z_height = motor_z->mm_to_microsteps(160),  // 160mm in steps
        .home_x = 0,
        .home_y = 0,
        .home_z = 0
    };

    exec_sub_fsm_init(&exec_sub_fsm, &exec_config);

    // Check if GCode is loaded in RAM
    if (!g_gcode_loaded || !g_gcode_buffer) {
        ESP_LOGE(TAG, "=== NO GCODE UPLOADED ===");
        ESP_LOGE(TAG, "Cannot execute - no GCode in RAM");
        ESP_LOGE(TAG, "Please upload GCode via POST /api/gcode/upload");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    ESP_LOGI(TAG, "=== EXECUTING FROM GCODE ===");
    ESP_LOGI(TAG, "GCode buffer: %d bytes in RAM", g_gcode_size);

    if (!exec_sub_fsm_load_gcode_from_ram(&exec_sub_fsm, g_gcode_buffer, g_gcode_size)) {
        ESP_LOGE(TAG, "Failed to load GCode from RAM");
        fsm_controller_post_event(fsm_handle, FSM_EVENT_DATA_ERROR);
        return false;
    }

    ESP_LOGI(TAG, "GCode parser initialized - starting execution");
    return true;
}

static bool on_execute_executing(void* user_data) {
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
    exec_sub_fsm_process_gcode(&exec_sub_fsm);

    if (exec_sub_fsm_get_state(&exec_sub_fsm) == EXEC_STATE_COMPLETE) {
        ESP_LOGI(TAG, "GCode execution complete: %d commands executed",
                 exec_sub_fsm_get_completed_count(&exec_sub_fsm));

        // Cleanup GCode resources
        exec_sub_fsm_cleanup_gcode(&exec_sub_fsm);

        fsm_controller_post_event(fsm_handle, FSM_EVENT_TASK_DONE);
    }

    return true;
}

static bool on_enter_normal_exit(void* user_data) {
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

static bool on_execute_normal_exit(void* user_data) {
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

static void init_fsm(void) {
    fsm_config_t config = {
        .tick_rate_ms = 100,
        .enable_logging = true,
        .enable_statistics = true,
        .target_temperature = 350.0f,
        .temperature_tolerance = 20.0f,
        .heating_timeout_ms = 60000,
        .calibration_timeout_ms = 30000,
        .safe_temperature = 150.0f,  // Safe handling temperature
        .cooldown_timeout_ms = 600000  // 10 minutes
    };

    fsm_handle = fsm_controller_init(&config);
    if (!fsm_handle) {
        ESP_LOGE(TAG, "FSM init failed");
        return;
    }

    // Register callbacks
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_IDLE, on_enter_idle, nullptr);
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_CALIBRATION, on_enter_calibration, nullptr);
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_READY, on_enter_ready, nullptr);
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_HEATING, on_enter_heating, nullptr);
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_EXECUTING, on_enter_executing, nullptr);
    fsm_controller_register_enter_callback(fsm_handle, FSM_STATE_NORMAL_EXIT, on_enter_normal_exit, nullptr);

    fsm_controller_register_execute_callback(fsm_handle, FSM_STATE_CALIBRATION, on_execute_calibration, nullptr);
    fsm_controller_register_execute_callback(fsm_handle, FSM_STATE_HEATING, on_execute_heating, nullptr);
    fsm_controller_register_execute_callback(fsm_handle, FSM_STATE_EXECUTING, on_execute_executing, nullptr);
    fsm_controller_register_execute_callback(fsm_handle, FSM_STATE_NORMAL_EXIT, on_execute_normal_exit, nullptr);

    if (!fsm_controller_start(fsm_handle)) {
        ESP_LOGE(TAG, "FSM start failed");
        return;
    }

    ESP_LOGI(TAG, "FSM initialized");
}

/**
 * @brief FSM processing task
 */
static void fsm_task(void* pvParameters) {
    while (1) {
        fsm_controller_process(fsm_handle);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void init_webserver() {
    ESP_LOGI(TAG, "Initializing WiFi Access Point...");
    wifi_manager_config_t wifi_config = {
        .ssid = "Паяйко",
        .channel = 1,
        .max_connections = 4
    };

    wifi_manager_handle_t wifi_handle = wifi_manager_init(&wifi_config);
    if (!wifi_handle) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    } else {
        ESP_LOGI(TAG, "WiFi AP started. SSID: %s, IP: %s",
                 wifi_config.ssid, wifi_manager_get_ip_address(wifi_handle));
    }

    // Initialize Web Server with FSM handle
    ESP_LOGI(TAG, "Initializing web server...");
    web_server_config_t web_config = {
        .port = 80,
        .max_uri_handlers = 24,
        .max_resp_headers = 8,
        .enable_websocket = true
    };

    web_server_handle_t web_handle = web_server_init(&web_config, fsm_handle);
    if (!web_handle) {
        ESP_LOGE(TAG, "Failed to initialize web server");
    } else {
        ESP_LOGI(TAG, "Web server started on port %d", web_config.port);
        ESP_LOGI(TAG, "Access web interface at: http://%s",
                 wifi_manager_get_ip_address(wifi_handle));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Automatic Soldering Station ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create mutex for GCode buffer protection
    g_gcode_mutex = xSemaphoreCreateMutex();
    if (!g_gcode_mutex) {
        ESP_LOGE(TAG, "Failed to create GCode mutex!");
    } else {
        ESP_LOGI(TAG, "GCode buffer mutex created");
    }

    init_motors();
    init_heating_system();
    init_fsm();
    init_webserver();

    xTaskCreate(fsm_task, "fsm_task", 4096, nullptr, 5, nullptr);

    ESP_LOGI(TAG, "System initialized");
    ESP_LOGI(TAG, "Waiting for commands from web interface...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
