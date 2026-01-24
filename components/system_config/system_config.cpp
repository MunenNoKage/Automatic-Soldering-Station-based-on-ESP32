/**
 * @file system_config.cpp
 * @brief Implementation of system configuration and initialization
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2026
 */

#include "system_config.h"
#include "esp_log.h"
#include "stepper_motor_hal.h"
#include "fsm_callbacks.h"
#include "sdkconfig.h"

static const char *TAG = "SYSTEM_CONFIG";

// ============================================================================
// Global Hardware Handles
// ============================================================================

StepperMotor* motor_x = nullptr;
StepperMotor* motor_y = nullptr;
StepperMotor* motor_z = nullptr;
StepperMotor* motor_s = nullptr;

soldering_iron_handle_t g_iron_handle = nullptr;
temperature_sensor_handle_t g_temp_sensor_handle = nullptr;
fsm_controller_handle_t g_fsm_handle = nullptr;

char* g_gcode_buffer = nullptr;
size_t g_gcode_size = 0;
bool g_gcode_loaded = false;
SemaphoreHandle_t g_gcode_mutex = nullptr;

execution_sub_fsm_t g_exec_sub_fsm;

// ============================================================================
// Configuration Storage
// ============================================================================

static system_config_t active_config;
static bool config_initialized = false;

// Default system configuration
static const system_config_t default_config = {
    // X-Axis Motor Configuration
    .motor_x_config = {
        .k_slope = 1.8,
        .minimal_step_delay_us = 245,
        .microsteps_in_mm = CONFIG_MOTOR_X_MICROSTEPS_IN_MM,
        .default_dir = STEPPER_DIR_COUNTERCLOCKWISE
    },

    // Y-Axis Motor Configuration
    .motor_y_config = {
        .k_slope = 1.8,
        .minimal_step_delay_us = 245,
        .microsteps_in_mm = CONFIG_MOTOR_Y_MICROSTEPS_IN_MM,
        .default_dir = STEPPER_DIR_CLOCKWISE
    },

    // Z-Axis Motor Configuration
    .motor_z_config = {
        .k_slope = 0.2,
        .minimal_step_delay_us = 350,
        .microsteps_in_mm = CONFIG_MOTOR_Z_MICROSTEPS_IN_MM,
        .default_dir = STEPPER_DIR_CLOCKWISE
    },

    // Solder Motor Configuration
    .motor_s_config = {
        .k_slope = 0.0,
        .minimal_step_delay_us = 600,
        .microsteps_in_mm = CONFIG_MOTOR_S_MICROSTEPS_IN_MM,
        .default_dir = STEPPER_DIR_CLOCKWISE
    },

    // Execution FSM Configuration
    .exec_fsm_config = {
        .safe_z_height_mm = 140.0,
        .soldering_z_height_mm = 160.0,
        .home_x = 0,
        .home_y = 0,
        .home_z = 0
    },

    // PID Configuration for temperature control
    .pid_config = {
        .kp = 10.0,
        .ki = 0.1,
        .kd = 0.5
    },

    // Temperature parameters
    .min_temperature = 20.0,
    .max_temperature = static_cast<double>(CONFIG_SOLDERING_IRON_MAX_TEMP),

    // FSM Configuration
    .tick_rate_ms = 100,
    .target_temperature = CONFIG_SOLDERING_IRON_DEFAULT_TEMP,
    .temperature_tolerance = 20.0f,
    .heating_timeout_ms = 600000,        // 10 minutes
    .calibration_timeout_ms = 10000,    // 10 seconds
    .safe_temperature = 450.0f,         // Safe handling temperature
    .cooldown_timeout_ms = 600000,      // 10 minutes

    // WiFi Configuration
    .wifi_ssid = CONFIG_WIFI_SSID,
    .wifi_channel = 1,
    .wifi_max_connections = 4,

    // Web Server Configuration
    .web_port = 80,
    .web_max_uri_handlers = 24,
    .web_max_resp_headers = 8,
    .web_enable_websocket = true
};

// ============================================================================
// Configuration Functions
// ============================================================================

const system_config_t* system_config_get_default(void) {
    return &default_config;
}

const system_config_t* system_config_get_active(void) {
    if (!config_initialized) {
        active_config = default_config;
        config_initialized = true;
    }
    return &active_config;
}

bool system_config_set(const system_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration pointer");
        return false;
    }

    active_config = *config;
    config_initialized = true;

    ESP_LOGI(TAG, "System configuration updated");
    return true;
}

// ============================================================================
// Initialization Functions
// ============================================================================

bool system_init_motors(void) {
    ESP_LOGI(TAG, "Initializing stepper motors...");

    const system_config_t* config = system_config_get_active();

    // X-Axis Motor
    stepper_motor_config_t config_x = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_X_MIN_ENDPOINT_PIN),
        .k_slope = config->motor_x_config.k_slope,
        .minimal_step_delay_us = config->motor_x_config.minimal_step_delay_us
    };
    motor_x = new StepperMotor(config_x, config->motor_x_config.microsteps_in_mm,
                                config->motor_x_config.default_dir);
    if (!motor_x || !motor_x->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize X-axis motor");
        return false;
    }
    ESP_LOGI(TAG, "X-axis motor initialized");

    // Y-Axis Motor
    stepper_motor_config_t config_y = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Y_MIN_ENDPOINT_PIN),
        .k_slope = config->motor_y_config.k_slope,
        .minimal_step_delay_us = config->motor_y_config.minimal_step_delay_us
    };
    motor_y = new StepperMotor(config_y, config->motor_y_config.microsteps_in_mm,
                                config->motor_y_config.default_dir);
    if (!motor_y || !motor_y->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize Y-axis motor");
        return false;
    }
    ESP_LOGI(TAG, "Y-axis motor initialized");

    // Z-Axis Motor
    stepper_motor_config_t config_z = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_ENABLE_PIN),
        .endpoint_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_Z_MIN_ENDPOINT_PIN),
        .k_slope = config->motor_z_config.k_slope,
        .minimal_step_delay_us = config->motor_z_config.minimal_step_delay_us
    };
    motor_z = new StepperMotor(config_z, config->motor_z_config.microsteps_in_mm,
                                config->motor_z_config.default_dir);
    if (!motor_z || !motor_z->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize Z-axis motor");
        return false;
    }
    ESP_LOGI(TAG, "Z-axis motor initialized");

    // Solder Supply Motor
    stepper_motor_config_t config_s = {
        .step_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_STEP_PIN),
        .dir_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_DIR_PIN),
        .enable_pin = static_cast<gpio_num_t>(CONFIG_MOTOR_S_ENABLE_PIN),
        .endpoint_pin = GPIO_NUM_NC,
        .k_slope = config->motor_s_config.k_slope,
        .minimal_step_delay_us = config->motor_s_config.minimal_step_delay_us
    };
    motor_s = new StepperMotor(config_s, config->motor_s_config.microsteps_in_mm,
                                config->motor_s_config.default_dir);
    if (!motor_s || !motor_s->isInitialized()) {
        ESP_LOGE(TAG, "Failed to initialize solder supply motor");
        return false;
    }
    ESP_LOGI(TAG, "Solder supply motor initialized");

    ESP_LOGI(TAG, "All motors initialized successfully");
    return true;
}

bool system_init_heating(void) {
    ESP_LOGI(TAG, "Initializing heating system...");

    const system_config_t* config = system_config_get_active();

    // Initialize temperature sensor (MAX6675)
    temperature_sensor_config_t temp_config = {
        .host_id = VSPI_HOST,
        .pin_miso = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_MISO_PIN),
        .pin_mosi = GPIO_NUM_NC,
        .pin_clk = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_CLK_PIN),
        .pin_cs = static_cast<gpio_num_t>(CONFIG_TEMP_SENSOR_CS_PIN),
        .dma_chan = 0,
        .clock_speed_hz = 2000000
    };

    g_temp_sensor_handle = temperature_sensor_hal_init(&temp_config);
    if (!g_temp_sensor_handle) {
        ESP_LOGE(TAG, "Failed to initialize temperature sensor");
        return false;
    }
    ESP_LOGI(TAG, "Temperature sensor initialized");

    // Initialize soldering iron PWM control
    soldering_iron_config_t iron_config = {
        .heater_pwm_pin = static_cast<gpio_num_t>(CONFIG_SOLDERING_IRON_PWM_PIN),
        .pwm_timer = LEDC_TIMER_0,
        .pwm_channel = LEDC_CHANNEL_0,
        .pwm_frequency = CONFIG_SOLDERING_IRON_PWM_FREQUENCY,
        .pwm_resolution = LEDC_TIMER_10_BIT,
        .max_temperature = config->max_temperature,
        .min_temperature = config->min_temperature
    };

    g_iron_handle = soldering_iron_hal_init(&iron_config);
    if (!g_iron_handle) {
        ESP_LOGE(TAG, "Failed to initialize soldering iron");
        return false;
    }

    // Set PID constants from configuration
    soldering_iron_hal_set_pid_constants(g_iron_handle,
                                          config->pid_config.kp,
                                          config->pid_config.ki,
                                          config->pid_config.kd);

    ESP_LOGI(TAG, "Soldering iron initialized with PID (Kp=%.1f, Ki=%.1f, Kd=%.1f)",
             config->pid_config.kp, config->pid_config.ki, config->pid_config.kd);

    return true;
}

bool system_init_fsm(void) {
    ESP_LOGI(TAG, "Initializing FSM controller...");

    const system_config_t* config = system_config_get_active();

    // Create FSM configuration from system config
    fsm_config_t fsm_config = {
        .tick_rate_ms = config->tick_rate_ms,
        .enable_logging = true,
        .enable_statistics = true,
        .target_temperature = config->target_temperature,
        .temperature_tolerance = config->temperature_tolerance,
        .heating_timeout_ms = config->heating_timeout_ms,
        .calibration_timeout_ms = config->calibration_timeout_ms,
        .safe_temperature = config->safe_temperature,
        .cooldown_timeout_ms = config->cooldown_timeout_ms
    };

    g_fsm_handle = fsm_controller_init(&fsm_config);
    if (!g_fsm_handle) {
        ESP_LOGE(TAG, "FSM initialization failed");
        return false;
    }

    // Initialize FSM callbacks module with hardware references
    fsm_callbacks_init(
        motor_x, motor_y, motor_z, motor_s,
        g_iron_handle, g_temp_sensor_handle,
        &g_exec_sub_fsm,
        &g_gcode_buffer, &g_gcode_size, &g_gcode_loaded,
        g_fsm_handle
    );

    // Register all FSM callbacks
    fsm_callbacks_register_all(g_fsm_handle);

    // Start FSM controller
    if (!fsm_controller_start(g_fsm_handle)) {
        ESP_LOGE(TAG, "FSM start failed");
        return false;
    }

    ESP_LOGI(TAG, "FSM controller initialized and started");
    return true;
}

bool system_init_network(void) {
    ESP_LOGI(TAG, "Initializing network services...");

    const system_config_t* config = system_config_get_active();

    // Initialize WiFi Access Point
    wifi_manager_config_t wifi_config = {
        .ssid = config->wifi_ssid,
        .channel = config->wifi_channel,
        .max_connections = config->wifi_max_connections
    };

    wifi_manager_handle_t wifi_handle = wifi_manager_init(&wifi_config);
    if (!wifi_handle) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return false;
    }

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, IP: %s",
             config->wifi_ssid, wifi_manager_get_ip_address(wifi_handle));

    // Initialize Web Server
    web_server_config_t web_config = {
        .port = config->web_port,
        .max_uri_handlers = config->web_max_uri_handlers,
        .max_resp_headers = config->web_max_resp_headers,
        .enable_websocket = config->web_enable_websocket
    };

    web_server_handle_t web_handle = web_server_init(&web_config, g_fsm_handle);
    if (!web_handle) {
        ESP_LOGE(TAG, "Failed to initialize web server");
        return false;
    }

    ESP_LOGI(TAG, "Web server started on port %d", config->web_port);
    ESP_LOGI(TAG, "Access web interface at: http://%s",
             wifi_manager_get_ip_address(wifi_handle));

    return true;
}

bool system_init_all(void) {
    ESP_LOGI(TAG, "=== System Initialization ===");

    // Create GCode buffer mutex
    g_gcode_mutex = xSemaphoreCreateMutex();
    if (!g_gcode_mutex) {
        ESP_LOGE(TAG, "Failed to create GCode mutex!");
        return false;
    }
    ESP_LOGI(TAG, "GCode buffer mutex created");

    // Initialize motors
    if (!system_init_motors()) {
        ESP_LOGE(TAG, "Motor initialization failed");
        return false;
    }

    // Initialize heating system
    if (!system_init_heating()) {
        ESP_LOGE(TAG, "Heating system initialization failed");
        return false;
    }

    // Initialize FSM controller
    if (!system_init_fsm()) {
        ESP_LOGE(TAG, "FSM initialization failed");
        return false;
    }

    // Initialize network services
    if (!system_init_network()) {
        ESP_LOGE(TAG, "Network initialization failed");
        return false;
    }

    ESP_LOGI(TAG, "=== System Initialization Complete ===");
    return true;
}

bool system_start_fsm_task(void) {
    BaseType_t ret = xTaskCreate(
        [](void* pvParameters) {
            while (1) {
                fsm_controller_process(g_fsm_handle);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        },
        "fsm_task",
        4096,
        nullptr,
        5,
        nullptr
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FSM task");
        return false;
    }

    ESP_LOGI(TAG, "FSM processing task started");
    return true;
}
