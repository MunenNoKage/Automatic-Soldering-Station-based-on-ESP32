/**
 * @file temperature_sensor_hal.c
 * @brief Implementation of HAL for MAX6675
 */

#include "temperature_sensor_hal.h"
#include <stdlib.h> // For malloc/free
#include "esp_log.h"

// Tag for logging
static const char *TAG = "MAX6675_HAL";

/**
 * @brief Internal handle structure
 */
struct temperature_sensor_handle_s
{
    temperature_sensor_config_t config; // Copy of configuration
    spi_device_handle_t spi_device;     // SPI device handle
    bool is_bus_initialized;            // Flag that we initialized the bus
};

temperature_sensor_handle_t temperature_sensor_hal_init(const temperature_sensor_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }

    // 1. Allocate memory for the handle
    temperature_sensor_handle_t handle = (temperature_sensor_handle_t)malloc(sizeof(struct temperature_sensor_handle_s));
    if (handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }

    // 2. Copy the configuration
    handle->config = *config;
    handle->spi_device = NULL;
    handle->is_bus_initialized = false;

    // 3. SPI bus configuration (from your `spi_mod.c`)
    spi_bus_config_t buscfg = {
        .miso_io_num = config->pin_miso,
        .mosi_io_num = config->pin_mosi,
        .sclk_io_num = config->pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2 // We need only 2 bytes (16 bits)
    };

    // 4. Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(config->host_id, &buscfg, config->dma_chan);
    if (ret != ESP_OK)
    {
        // Possibly, the bus is already initialized. This is OK.
        if (ret == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(TAG, "SPI bus (host: %d) already initialized.", config->host_id);
            handle->is_bus_initialized = false; // We do not "own" the bus
        }
        else
        {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
            free(handle);
            return NULL;
        }
    }
    else
    {
        handle->is_bus_initialized = true; // We initialized the bus
    }

    // 5. Device configuration (from your `spi_mod.c`)
    spi_device_interface_config_t devCfg = {
        .mode = 0, // SPI Mode 0 (CPOL=0, CPHA=0) - correct for MAX6675
        .clock_speed_hz = config->clock_speed_hz,
        .spics_io_num = config->pin_cs,
        .queue_size = 1 // We need only one transaction in the queue
    };

    // 6. Add device to bus
    ret = spi_bus_add_device(config->host_id, &devCfg, &handle->spi_device);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        if (handle->is_bus_initialized)
        {
            spi_bus_free(config->host_id);
        }
        free(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "MAX6675 HAL initialized. Host: %d, CS: %d", config->host_id, config->pin_cs);
    return handle;
}

void temperature_sensor_hal_deinit(temperature_sensor_handle_t handle)
{
    if (handle == NULL)
        return;

    // 1. Remove device from bus
    if (handle->spi_device)
    {
        spi_bus_remove_device(handle->spi_device);
    }

    // 2. Free the bus, BUT ONLY IF WE initialized it
    if (handle->is_bus_initialized)
    {
        spi_bus_free(handle->config.host_id);
    }

    // 3. Free memory
    free(handle);
    ESP_LOGI(TAG, "Temperature sensor HAL deinitialized");
}

esp_err_t temperature_sensor_hal_read_raw(temperature_sensor_handle_t handle, uint16_t *out_raw_data)
{
    if (handle == NULL || handle->spi_device == NULL || out_raw_data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t data = 0; // Raw data will be here

    // 1. Prepare transaction (from your `main.c`)
    spi_transaction_t tM = {
        .tx_buffer = NULL,  // Nothing to send
        .rx_buffer = &data, // Receive data here
        .length = 16,       // 16 bits
        .rxlength = 16,     // Expect 16 bits
    };

    // 2. Execute transaction (blocking mode)
    // This is a cleaner replacement for acquire/transmit/release
    esp_err_t ret = spi_device_polling_transmit(handle->spi_device, &tM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_device_polling_transmit failed: %s", esp_err_to_name(ret));
        return ret; // Return SPI error
    }

    // 3. Correct byte order (from your `main.c`)
    *out_raw_data = SPI_SWAP_DATA_RX(data, 16);

    return ESP_OK;
}

esp_err_t temperature_sensor_hal_read_temperature(temperature_sensor_handle_t handle, double *out_temp)
{
    if (out_temp == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw_data = 0;

    // 1. Get raw data
    esp_err_t ret = temperature_sensor_hal_read_raw(handle, &raw_data);
    if (ret != ESP_OK)
    {
        *out_temp = 0.0; // Or NAN
        return ret;      // SPI error
    }

    // 2. Analyze raw data (logic from your `main.c`)

    // Check bit D2 (Open circuit / thermocouple not connected)
    if (raw_data & (1 << 2))
    {
        // ESP_LOGW(TAG, "Thermocouple is not connected (Open Circuit)");
        *out_temp = 0.0; // Return 0, but with error status
        return ESP_FAIL; // Use ESP_FAIL as "sensor error"
    }

    // 3. Calculate temperature
    int16_t temp_data = (int16_t)raw_data;

    // Discard 3 lower bits (D0, D1, D2)
    temp_data >>= 3;

    // Convert to degrees C (resolution 0.25 C)
    *out_temp = (double)temp_data * 0.25;

    return ESP_OK;
}