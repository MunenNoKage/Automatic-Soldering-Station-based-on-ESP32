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
#include "fsm_callbacks.h"

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
// Note: Not static to allow access from web_server for origin setting
execution_sub_fsm_t exec_sub_fsm;

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
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_MIN_ENDPOINT_PIN),
        .k_slope = 1.8,
        .minimal_step_delay_us = 245
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
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_MIN_ENDPOINT_PIN),
        .k_slope = 1.8,
        .minimal_step_delay_us = 245
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
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_MIN_ENDPOINT_PIN),
        .k_slope = 0.2,
        .minimal_step_delay_us = 350
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
        .endpoint_pin = GPIO_NUM_NC,
        .k_slope = 0.0,
        .minimal_step_delay_us = 600
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

static void init_fsm(void) {
    fsm_config_t config = {
        .tick_rate_ms = 100,
        .enable_logging = true,
        .enable_statistics = true,
        .target_temperature = CONFIG_SOLDERING_IRON_DEFAULT_TEMP,
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

    // Initialize the FSM callbacks module with hardware references
    fsm_callbacks_init(
        motor_x, motor_y, motor_z, motor_s,
        iron_handle, temp_sensor_handle,
        &exec_sub_fsm,
        &g_gcode_buffer, &g_gcode_size, &g_gcode_loaded,
        fsm_handle
    );

    // Register all FSM callbacks
    fsm_callbacks_register_all(fsm_handle);

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
        .ssid = CONFIG_WIFI_SSID,
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
