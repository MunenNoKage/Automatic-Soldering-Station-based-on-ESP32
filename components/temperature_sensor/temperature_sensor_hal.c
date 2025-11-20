/**
 * @file temperature_sensor_hal.c
 * @brief Реалізація HAL для MAX6675
 */

#include "temperature_sensor_hal.h"
#include <stdlib.h> // Для malloc/free
#include "esp_log.h"

// Тег для логування
static const char *TAG = "MAX6675_HAL";

/**
 * @brief Внутрішня структура "ручки"
 */
struct temperature_sensor_handle_s
{
    temperature_sensor_config_t config; // Копія конфігурації
    spi_device_handle_t spi_device;     // "Ручка" SPI пристрою
    bool is_bus_initialized;            // Прапорець, що ми ініціалізували шину
};

temperature_sensor_handle_t temperature_sensor_hal_init(const temperature_sensor_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }

    // 1. Виділяємо пам'ять під "ручку"
    temperature_sensor_handle_t handle = (temperature_sensor_handle_t)malloc(sizeof(struct temperature_sensor_handle_s));
    if (handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }

    // 2. Копіюємо конфігурацію
    handle->config = *config;
    handle->spi_device = NULL;
    handle->is_bus_initialized = false;

    // 3. Конфігурація шини SPI (з вашого `spi_mod.c`)
    spi_bus_config_t buscfg = {
        .miso_io_num = config->pin_miso,
        .mosi_io_num = config->pin_mosi,
        .sclk_io_num = config->pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2 // Нам потрібно лише 2 байти (16 біт)
    };

    // 4. Ініціалізуємо SPI шину
    esp_err_t ret = spi_bus_initialize(config->host_id, &buscfg, config->dma_chan);
    if (ret != ESP_OK)
    {
        // Можливо, шина вже ініціалізована. Це ОК.
        if (ret == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(TAG, "SPI bus (host: %d) already initialized.", config->host_id);
            handle->is_bus_initialized = false; // Ми не "володіємо" шиною
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
        handle->is_bus_initialized = true; // Ми ініціалізували шину
    }

    // 5. Конфігурація пристрою (з вашого `spi_mod.c`)
    spi_device_interface_config_t devCfg = {
        .mode = 0, // SPI Mode 0 (CPOL=0, CPHA=0) - коректно для MAX6675
        .clock_speed_hz = config->clock_speed_hz,
        .spics_io_num = config->pin_cs,
        .queue_size = 1 // Нам потрібна лише одна транзакція в черзі
    };

    // 6. Додаємо пристрій до шини
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

    // 1. Видаляємо пристрій з шини
    if (handle->spi_device)
    {
        spi_bus_remove_device(handle->spi_device);
    }

    // 2. Звільняємо шину, АЛЕ ТІЛЬКИ ЯКЩО МИ її ініціалізували
    if (handle->is_bus_initialized)
    {
        spi_bus_free(handle->config.host_id);
    }

    // 3. Звільняємо пам'ять
    free(handle);
    ESP_LOGI(TAG, "Temperature sensor HAL deinitialized");
}

esp_err_t temperature_sensor_hal_read_raw(temperature_sensor_handle_t handle, uint16_t *out_raw_data)
{
    if (handle == NULL || handle->spi_device == NULL || out_raw_data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t data = 0; // Тут будуть сирі дані

    // 1. Готуємо транзакцію (з вашого `main.c`)
    spi_transaction_t tM = {
        .tx_buffer = NULL,  // Нічого не надсилаємо
        .rx_buffer = &data, // Отримуємо дані сюди
        .length = 16,       // 16 біт
        .rxlength = 16,     // Очікуємо 16 біт
    };

    // 2. Виконуємо транзакцію (блокуючий режим)
    // Це чистіша заміна для acquire/transmit/release
    esp_err_t ret = spi_device_polling_transmit(handle->spi_device, &tM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_device_polling_transmit failed: %s", esp_err_to_name(ret));
        return ret; // Повертаємо помилку SPI
    }

    // 3. Коригуємо порядок байтів (з вашого `main.c`)
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

    // 1. Отримуємо "сирі" дані
    esp_err_t ret = temperature_sensor_hal_read_raw(handle, &raw_data);
    if (ret != ESP_OK)
    {
        *out_temp = 0.0; // Або NAN
        return ret;      // Помилка SPI
    }

    // 2. Аналізуємо "сирі" дані (логіка з вашого `main.c`)

    // Перевіряємо біт D2 (Open circuit / термопара не підключена)
    if (raw_data & (1 << 2))
    {
        // ESP_LOGW(TAG, "Thermocouple is not connected (Open Circuit)");
        *out_temp = 0.0; // Повертаємо 0, але зі статусом помилки
        return ESP_FAIL; // Використовуємо ESP_FAIL як "помилку сенсора"
    }

    // 3. Розраховуємо температуру
    int16_t temp_data = (int16_t)raw_data;

    // Відкидаємо 3 молодших біти (D0, D1, D2)
    temp_data >>= 3;

    // Конвертуємо у градуси C (роздільна здатність 0.25 C)
    *out_temp = (double)temp_data * 0.25;

    return ESP_OK;
}