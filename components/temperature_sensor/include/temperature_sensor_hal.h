/**
 * @file temperature_sensor_hal.h
 * @brief Hardware Abstraction Layer for MAX6675 K-Type thermocouple
 * * Надає С-інтерфейс для зчитування температури з MAX6675.
 * Використовує ESP32 SPI master driver.
 */

#ifndef TEMPERATURE_SENSOR_HAL_H
#define TEMPERATURE_SENSOR_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h" // Потрібен для SPI
#include "driver/gpio.h"       // Потрібен для gpio_num_t
#include "esp_err.h"           // Потрібен для esp_err_t

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Конфігурація SPI-сенсора (MAX6675)
     */
    typedef struct
    {
        spi_host_device_t host_id; // SPI хост (напр. VSPI_HOST)
        gpio_num_t pin_miso;       // Пін MISO
        gpio_num_t pin_mosi;       // Пін MOSI (можна -1)
        gpio_num_t pin_clk;         // Пін SCLK
        gpio_num_t pin_cs;          // Пін Chip Select
        int dma_chan;              // Канал DMA (0 = вимкнено)
        int clock_speed_hz;        // Швидкість SPI (напр. 2*1000*1000)
    } temperature_sensor_config_t;

    /**
     * @brief "Ручка" сенсора (непрозорий вказівник)
     * Зберігає внутрішній стан, вкл. spi_device_handle_t
     */
    typedef struct temperature_sensor_handle_s *temperature_sensor_handle_t;

    /**
     * @brief Ініціалізація SPI-шини та сенсора MAX6675
     * * Налаштовує SPI шину та додає до неї пристрій MAX6675.
     * @param config Вказівник на структуру конфігурації.
     * @return "Ручка" сенсора, або NULL у разі помилки.
     */
    temperature_sensor_handle_t temperature_sensor_hal_init(const temperature_sensor_config_t *config);

    /**
     * @brief Деініціалізація сенсора та звільнення SPI-шини
     * * @param handle "Ручка" сенсора, отримана з hal_init.
     */
    void temperature_sensor_hal_deinit(temperature_sensor_handle_t handle);

    /**
     * @brief Зчитування температури в градусах Цельсія
     *
     * @param handle "Ручка" сенсора.
     * @param[out] out_temp Вказівник, куди буде записана температура.
     * @return esp_err_t:
     * - ESP_OK: Успіх, температура записана в out_temp.
     * - ESP_FAIL: Помилка сенсора (термопара не підключена).
     * - Інші коди помилок: Помилка комунікації SPI.
     */
    esp_err_t temperature_sensor_hal_read_temperature(temperature_sensor_handle_t handle, double *out_temp);

    /**
     * @brief (Опційно) Зчитування "сирих" 16-бітних даних з сенсора
     *
     * @param handle "Ручка" сенсора.
     * @param[out] out_raw_data Вказівник, куди будуть записані сирі дані.
     * @return esp_err_t ESP_OK або помилка SPI.
     */
    esp_err_t temperature_sensor_hal_read_raw(temperature_sensor_handle_t handle, uint16_t *out_raw_data);

#ifdef __cplusplus
}
#endif

#endif // TEMPERATURE_SENSOR_HAL_H
