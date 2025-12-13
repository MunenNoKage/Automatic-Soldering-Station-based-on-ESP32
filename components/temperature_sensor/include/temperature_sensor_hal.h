/**
 * @file temperature_sensor_hal.h
 * @brief Hardware Abstraction Layer for MAX6675 K-Type thermocouple
 * Provides a C-interface for reading temperature from MAX6675.
 * Uses ESP32 SPI master driver.
 */

#ifndef TEMPERATURE_SENSOR_HAL_H
#define TEMPERATURE_SENSOR_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h" // Needed SPI
#include "driver/gpio.h"       // Needed for gpio_num_t
#include "esp_err.h"           // Needed for esp_err_t

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Configuration of SPI sensor (MAX6675)
     */
    typedef struct
    {
        spi_host_device_t host_id; // SPI host (e.g. VSPI_HOST)
        gpio_num_t pin_miso;       // MISO pin
        gpio_num_t pin_mosi;       // MOSI pin (can be -1)
        gpio_num_t pin_clk;         // SCLK pin
        gpio_num_t pin_cs;          // Chip Select pin
        int dma_chan;              // DMA channel (0 = disabled)
        int clock_speed_hz;        // SPI speed (e.g. 2*1000*1000)
    } temperature_sensor_config_t;

    /**
     * @brief Sensor handle (opaque pointer)
     * Stores internal state, incl. spi_device_handle_t
     */
    typedef struct temperature_sensor_handle_s *temperature_sensor_handle_t;

    /**
     * @brief Initialization of SPI bus and MAX6675 sensor
     * Configures SPI bus and adds MAX6675 device to it.
     * @param config Pointer to configuration structure.
     * @return Sensor handle, or NULL on error.
     */
    temperature_sensor_handle_t temperature_sensor_hal_init(const temperature_sensor_config_t *config);

    /**
     * @brief Deinitialization of sensor and freeing SPI bus
     * @param handle Sensor handle obtained from hal_init.
     */
    void temperature_sensor_hal_deinit(temperature_sensor_handle_t handle);

    /**
     * @brief Reading temperature in degrees Celsius
     *
     * @param handle Sensor handle.
     * @param[out] out_temp Pointer where temperature will be written.
     * @return esp_err_t:
     * - ESP_OK: Success, temperature written to out_temp.
     * - ESP_FAIL: Sensor error (thermocouple not connected).
     * - Other error codes: SPI communication error.
     */
    esp_err_t temperature_sensor_hal_read_temperature(temperature_sensor_handle_t handle, double *out_temp);

    /**
     * @brief (Optional) Reading raw 16-bit data from sensor
     *
     * @param handle Sensor handle.
     * @param[out] out_raw_data Pointer where raw data will be written.
     * @return esp_err_t ESP_OK or SPI error.
     */
    esp_err_t temperature_sensor_hal_read_raw(temperature_sensor_handle_t handle, uint16_t *out_raw_data);

#ifdef __cplusplus
}
#endif

#endif // TEMPERATURE_SENSOR_HAL_H
