/**
 * @file soldering_iron_hal.c
 * @brief Реалізація HAL для паяльника з ПІД-регулятором
 */

#include "soldering_iron_hal.h"
#include <stdlib.h> // Для malloc/free
#include <string.h> // Для memset
#include <math.h>   // Для fmin, fmax
#include "esp_log.h"
#include "esp_timer.h" // Для точного delta-time у ПІД

static const char *TAG = "SOLDERING_IRON_HAL";

// --- КОНСТАНТИ ПІД-РЕГУЛЯТОРА (ВИМОГА ТЮНІНГУ) ---
// Вам доведеться підібрати ці значення для вашого паяльника.
// Почніть з Kp, потім додайте Ki, і в кінці Kd.
#define DEFAULT_PID_KP 15.0 // Пропорційний (наскільки сильно реагувати на помилку)
#define DEFAULT_PID_KI 0.1  // Інтегральний (як швидко виправляти малі помилки)
#define DEFAULT_PID_KD 0.0  // Диференціальний (наскільки сильно гасити коливання)

// Межі для інтегральної частини (запобігає "Integral Windup")
#define PID_INTEGRAL_MIN -50.0
#define PID_INTEGRAL_MAX 50.0

/**
 * @brief Внутрішня структура "ручки" (handle)
 * Зберігає весь стан паяльника
 */
struct soldering_iron_handle_s
{
    soldering_iron_config_t config; // Копія конфігурації

    // Стан PWM
    uint32_t max_duty_value;  // Максимальне значення (напр. 1023 для 10 біт)
    double current_power_pct; // Поточна потужність (0.0 - 100.0)
    bool is_enabled;          // Чи увімкнений нагрів

    // Стан контролера
    double target_temperature;

    // Змінні стану ПІД-регулятора
    double pid_kp;
    double pid_ki;
    double pid_kd;
    double pid_integral;      // Накопичена інтегральна помилка
    double pid_last_error;    // Попередня помилка (для D)
    int64_t pid_last_time_us; // Час останнього розрахунку (у мікросекундах)
};

// --- Приватні функції ---

/**
 * @brief Встановлює "сиру" потужність PWM
 */
static void _set_pwm_duty_raw(soldering_iron_handle_t handle, uint32_t raw_duty)
{
    if (handle == NULL)
        return;

    // Встановлюємо нове значення
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel, raw_duty); // <--- ВИПРАВЛЕНО
    // Застосовуємо його
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel);
}

// --- Реалізація публічних функцій ---

soldering_iron_handle_t soldering_iron_hal_init(const soldering_iron_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }

    // 1. Виділяємо пам'ять під ручку
    soldering_iron_handle_t handle = (soldering_iron_handle_t)malloc(sizeof(struct soldering_iron_handle_s));
    if (handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }
    memset(handle, 0, sizeof(struct soldering_iron_handle_s)); // Обнуляємо

    // 2. Зберігаємо конфігурацію
    handle->config = *config;
    handle->is_enabled = false;
    handle->target_temperature = 0.0;
    handle->current_power_pct = 0.0;

    // Розраховуємо максимальне значення duty
    handle->max_duty_value = (1 << config->pwm_resolution) - 1;

    // 3. Налаштовуємо ПІД-константи (з #define)
    handle->pid_kp = DEFAULT_PID_KP;
    handle->pid_ki = DEFAULT_PID_KI;
    handle->pid_kd = DEFAULT_PID_KD;
    handle->pid_integral = 0.0;
    handle->pid_last_error = 0.0;
    handle->pid_last_time_us = esp_timer_get_time();
    ESP_LOGW(TAG, "PID constants set: Kp=%.2f, Ki=%.2f, Kd=%.2f. PLEASE TUNE THEM!",
             handle->pid_kp, handle->pid_ki, handle->pid_kd);

    // 4. Ініціалізуємо LEDC (PWM) таймер
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // (або HIGH, залежить від чіпа/настройки)
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

    // 5. Ініціалізуємо LEDC (PWM) канал
    ledc_channel_config_t channel_conf = {
        .gpio_num = config->heater_pwm_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->pwm_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->pwm_timer,
        .duty = 0, // Початкова потужність 0
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

    // Вимикаємо PWM
    ledc_stop(LEDC_LOW_SPEED_MODE, handle->config.pwm_channel, 0);
    // Звільняємо пам'ять
    free(handle);
    ESP_LOGI(TAG, "Soldering iron HAL deinitialized");
}

void soldering_iron_hal_set_power(soldering_iron_handle_t handle, double duty_cycle)
{
    if (handle == NULL)
        return;

    // 1. Обмежуємо потужність (0% - 100%)
    double clamped_power = fmax(0.0, fmin(100.0, duty_cycle));

    // 2. Зберігаємо стан
    handle->current_power_pct = clamped_power;

    // 3. Розраховуємо "сире" значення для ШІМ
    uint32_t raw_duty = (uint32_t)((clamped_power / 100.0) * (double)handle->max_duty_value);

    // 4. Встановлюємо потужність, лише якщо нагрів увімкнено
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

    // Обмежуємо температуру заданими межами
    double clamped_temp = fmax(handle->config.min_temperature,
                               fmin(handle->config.max_temperature, temperature));

    // Якщо ціль змінилася, скидаємо ПІД
    if (clamped_temp != handle->target_temperature)
    {
        ESP_LOGI(TAG, "Setting target temperature: %.2f C", clamped_temp);
        handle->target_temperature = clamped_temp;
        // Скидаємо інтеграл та "минулу помилку", щоб уникнути стрибків
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
        // Якщо вимикаємо, негайно ставимо потужність на 0
        soldering_iron_hal_set_power(handle, 0.0);
    }
    else
    {
        // Якщо вмикаємо, скидаємо ПІД для чистого старту
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

    // 1. Якщо нагрів вимкнено або ціль 0 - вимикаємо і виходимо
    if (!handle->is_enabled || handle->target_temperature <= 0.0)
    {
        if (handle->current_power_pct > 0.0)
        {
            soldering_iron_hal_set_power(handle, 0.0);
        }
        return;
    }

    // --- РОЗРАХУНОК ПІД ---

    // 2. Розраховуємо часовий інтервал (Delta Time)
    int64_t now_us = esp_timer_get_time();
    double dt_sec = (double)(now_us - handle->pid_last_time_us) / 1000000.0;
    // (Якщо dt занадто малий, пропускаємо цикл, щоб уникнути ділення на 0)
    if (dt_sec < 0.001)
    {
        return;
    }
    handle->pid_last_time_us = now_us;

    // 3. Розраховуємо помилку
    double error = handle->target_temperature - current_temperature;

    // 4. P (Пропорційна частина)
    double p_out = handle->pid_kp * error;

    // 5. I (Інтегральна частина)
    handle->pid_integral += (error * dt_sec);
    // Обмежуємо інтеграл (anti-windup)
    handle->pid_integral = fmax(PID_INTEGRAL_MIN, fmin(PID_INTEGRAL_MAX, handle->pid_integral));
    double i_out = handle->pid_ki * handle->pid_integral;

    // 6. D (Диференціальна частина)
    double derivative = (error - handle->pid_last_error) / dt_sec;
    handle->pid_last_error = error;
    double d_out = handle->pid_kd * derivative;

    // 7. Загальна вихідна потужність (0.0 - 100.0)
    double output_power = p_out + i_out + d_out;

    // 8. Обмежуємо вихід (0% - 100%)
    output_power = fmax(0.0, fmin(100.0, output_power));

    // 9. Застосовуємо розраховану потужність
    soldering_iron_hal_set_power(handle, output_power);

    // (Для дебагу ПІД-регулятора можна виводити це в лог)
    // ESP_LOGI(TAG, "Tgt: %.1f, Cur: %.1f, Err: %.1f, P: %.1f, I: %.1f, D: %.1f, Out: %.1f%%",
    //          handle->target_temperature, current_temperature, error,
    //          p_out, i_out, d_out, output_power);
}

void soldering_iron_hal_set_pid_constants(soldering_iron_handle_t handle, double kp, double ki, double kd)
{
    if (handle == NULL)
        return;

    // Встановлюємо нові константи
    handle->pid_kp = kp;
    handle->pid_ki = ki;
    handle->pid_kd = kd;

    // Скидаємо ПІД (особливо інтеграл) для чистого старту
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
