/**
 * @file soldering_iron_hal.c
 * @brief Implementation of HAL for soldering iron with PID controller
 */

#include "soldering_iron_hal.h"
#include <stdlib.h> // For malloc/free
#include <string.h> // For memset
#include <math.h>   // For fmin, fmax
#include "esp_log.h"
#include "esp_timer.h" // For precise delta-time in PID

static const char *TAG = "SOLDERING_IRON_HAL";

// --- PID CONTROLLER CONSTANTS (REQUIRES TUNING) ---
// You will have to tune these values for your soldering iron.
// Start with Kp, then add Ki, and finally Kd.
#define DEFAULT_PID_KP 15.0 // Proportional (how strongly to react to error)
#define DEFAULT_PID_KI 0.1  // Integral (how quickly to correct small errors)
#define DEFAULT_PID_KD 0.0  // Derivative (how strongly to damp oscillations)

// Limits for the integral part (prevents "Integral Windup")
#define PID_INTEGRAL_MIN -50.0
#define PID_INTEGRAL_MAX 50.0

/**
 * @brief Internal handle structure
 * Stores the entire state of the soldering iron
 */
struct soldering_iron_handle_s
{
    soldering_iron_config_t config; // Copy of configuration

    // PWM state
    uint32_t max_duty_value;  // Maximum value (e.g. 1023 for 10 bits)
    double current_power_pct; // Current power (0.0 - 100.0)
    bool is_enabled;          // Whether heating is enabled

    // Controller state
    double target_temperature;

    // PID controller state variables
    double pid_kp;
    double pid_ki;
    double pid_kd;
    double pid_integral;      // Accumulated integral error
    double pid_last_error;    // Previous error (for D)
    int64_t pid_last_time_us; // Time of last calculation (in microseconds)
};

// --- Private functions ---

/**
 * @brief Sets raw PWM power
 */
static void _set_pwm_duty_raw(soldering_iron_handle_t handle, uint32_t raw_duty)
{
    if (handle == NULL)
        return;

    // Set new value
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel, raw_duty); // <--- FIXED
    // Apply it
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel);
}

// --- Implementation of public functions ---

soldering_iron_handle_t soldering_iron_hal_init(const soldering_iron_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }

    // 1. Allocate memory for handle
    soldering_iron_handle_t handle = (soldering_iron_handle_t)malloc(sizeof(struct soldering_iron_handle_s));
    if (handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }
    memset(handle, 0, sizeof(struct soldering_iron_handle_s)); // Zero out

    // 2. Store configuration
    handle->config = *config;
    handle->is_enabled = false;
    handle->target_temperature = 0.0;
    handle->current_power_pct = 0.0;

    // Calculate maximum duty value
    handle->max_duty_value = (1 << config->pwm_resolution) - 1;

    // 3. Configure PID constants (from #define)
    handle->pid_kp = DEFAULT_PID_KP;
    handle->pid_ki = DEFAULT_PID_KI;
    handle->pid_kd = DEFAULT_PID_KD;
    handle->pid_integral = 0.0;
    handle->pid_last_error = 0.0;
    handle->pid_last_time_us = esp_timer_get_time();
    ESP_LOGW(TAG, "PID constants set: Kp=%.2f, Ki=%.2f, Kd=%.2f. PLEASE TUNE THEM!",
             handle->pid_kp, handle->pid_ki, handle->pid_kd);

    // 4. Initialize LEDC (PWM) timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // (or HIGH, depends on chip/settings)
        .duty_resolution = config->pwm_resolution,
        .timer_num = config->pwm_timer,
        .freq_hz = config->pwm_frequency,
        .clk_cfg = LEDC_AUTO_CLK};
    if (ledc_timer_config(&timer_conf) != ESP_OK)
    {
        ESP_LOGE(TAG, "ledc_timer_config failed");
        free(handle);
        return NULL;
    }

    // 5. Initialize LEDC (PWM) channel
    ledc_channel_config_t channel_conf = {
        .gpio_num = config->heater_pwm_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->pwm_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->pwm_timer,
        .duty = 0, // Initial power 0
        .hpoint = 0};
    if (ledc_channel_config(&channel_conf) != ESP_OK)
    {
        ESP_LOGE(TAG, "ledc_channel_config failed");
        free(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "Soldering iron HAL initialized on pin %d", config->heater_pwm_pin);
    return handle;
}

void soldering_iron_hal_deinit(soldering_iron_handle_t handle)
{
    if (handle == NULL)
        return;

    // Disable PWM
    ledc_stop(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel, 0);
    // Free memory
    free(handle);
    ESP_LOGI(TAG, "Soldering iron HAL deinitialized");
}

void soldering_iron_hal_set_power(soldering_iron_handle_t handle, double duty_cycle)
{
    if (handle == NULL)
        return;

    // 1. Limit power (0% - 100%)
    double clamped_power = fmax(0.0, fmin(100.0, duty_cycle));

    // 2. Save state
    handle->current_power_pct = clamped_power;

    // 3. Calculate "raw" value for PWM
    uint32_t raw_duty = (uint32_t)((clamped_power / 100.0) * (double)handle->max_duty_value);

    // 4. Set power only if heating is enabled
    if (handle->is_enabled)
    {
        _set_pwm_duty_raw(handle, raw_duty);
    }
    else
    {
        _set_pwm_duty_raw(handle, 0);
    }
}

void soldering_iron_hal_set_target_temperature(soldering_iron_handle_t handle, double temperature)
{
    if (handle == NULL)
        return;

    // Limit temperature within specified bounds
    double clamped_temp = fmax(handle->config.min_temperature,
                               fmin(handle->config.max_temperature, temperature));

    // If target changed, reset PID
    if (clamped_temp != handle->target_temperature)
    {
        ESP_LOGI(TAG, "Setting target temperature: %.2f C", clamped_temp);
        handle->target_temperature = clamped_temp;
        // Reset integral and "previous error" to avoid jumps
        handle->pid_integral = 0.0;
        handle->pid_last_error = 0.0;
        handle->pid_last_time_us = esp_timer_get_time();
    }
}

double soldering_iron_hal_get_target_temperature(soldering_iron_handle_t handle)
{
    if (handle == NULL)
        return 0.0;
    return handle->target_temperature;
}

void soldering_iron_hal_set_enable(soldering_iron_handle_t handle, bool enable)
{
    if (handle == NULL)
        return;
    handle->is_enabled = enable;

    if (!enable)
    {
        // If disabling, immediately set power to 0
        soldering_iron_hal_set_power(handle, 0.0);
    }
    else
    {
        // If enabling, reset PID for clean start
        handle->pid_integral = 0.0;
        handle->pid_last_error = 0.0;
        handle->pid_last_time_us = esp_timer_get_time();
    }
}

double soldering_iron_hal_get_power(soldering_iron_handle_t handle)
{
    if (handle == NULL)
        return 0.0;
    return handle->current_power_pct;
}

void soldering_iron_hal_update_control(soldering_iron_handle_t handle, double current_temperature)
{
    if (handle == NULL)
        return;

    // 1. If heating is disabled or target is 0 - disable and exit
    if (!handle->is_enabled || handle->target_temperature <= 0.0)
    {
        if (handle->current_power_pct > 0.0)
        {
            soldering_iron_hal_set_power(handle, 0.0);
        }
        return;
    }

    // --- PID CALCULATION ---

    // 2. Calculate time interval (Delta Time)
    int64_t now_us = esp_timer_get_time();
    double dt_sec = (double)(now_us - handle->pid_last_time_us) / 1000000.0;
    // (If dt is too small, skip cycle to avoid division by 0)
    if (dt_sec < 0.001)
    {
        return;
    }
    handle->pid_last_time_us = now_us;

    // 3. Calculate error
    double error = handle->target_temperature - current_temperature;

    // 4. P (Proportional part)
    double p_out = handle->pid_kp * error;

    // 5. I (Integral part)
    handle->pid_integral += (error * dt_sec);
    // Limit integral (anti-windup)
    handle->pid_integral = fmax(PID_INTEGRAL_MIN, fmin(PID_INTEGRAL_MAX, handle->pid_integral));
    double i_out = handle->pid_ki * handle->pid_integral;

    // 6. D (Derivative part)
    double derivative = (error - handle->pid_last_error) / dt_sec;
    handle->pid_last_error = error;
    double d_out = handle->pid_kd * derivative;

    // 7. Total output power (0.0 - 100.0)
    double output_power = p_out + i_out + d_out;

    // 8. Limit output (0% - 100%)
    output_power = fmax(0.0, fmin(100.0, output_power));

    // 9. Apply calculated power
    soldering_iron_hal_set_power(handle, output_power);

    // (For PID controller debugging, this can be output to log)
    // ESP_LOGI(TAG, "Tgt: %.1f, Cur: %.1f, Err: %.1f, P: %.1f, I: %.1f, D: %.1f, Out: %.1f%%",
    //          handle->target_temperature, current_temperature, error,
    //          p_out, i_out, d_out, output_power);
}

void soldering_iron_hal_set_pid_constants(soldering_iron_handle_t handle, double kp, double ki, double kd)
{
    if (handle == NULL)
        return;

    // Set new constants
    handle->pid_kp = kp;
    handle->pid_ki = ki;
    handle->pid_kd = kd;

    // Reset PID (especially integral) for clean start
    handle->pid_integral = 0.0;
    handle->pid_last_error = 0.0;
    handle->pid_last_time_us = esp_timer_get_time();

    ESP_LOGW(TAG, "New PID constants set: Kp=%.2f, Ki=%.2f, Kd=%.2f",
             handle->pid_kp, handle->pid_ki, handle->pid_kd);
}

void soldering_iron_hal_get_pid_constants(soldering_iron_handle_t handle, double *kp, double *ki, double *kd)
{
    if (handle == NULL || kp == NULL || ki == NULL || kd == NULL)
    {
        if (kp)
            *kp = 0.0;
        if (ki)
            *ki = 0.0;
        if (kd)
            *kd = 0.0;
        return;
    }

    *kp = handle->pid_kp;
    *ki = handle->pid_ki;
    *kd = handle->pid_kd;
}
