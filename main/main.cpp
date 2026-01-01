/**
 * @file main.cpp
 * @brief Main application entry point for Automatic Soldering Station
 *
 * This file contains the minimal app_main() function that initializes
 * all system components through the system_config module.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2026
 */

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_config.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Automatic Soldering Station ===");
    ESP_LOGI(TAG, "Starting system initialization...");

    // Initialize NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash initialized");

    // Initialize all system components
    if (!system_init_all()) {
        ESP_LOGE(TAG, "System initialization failed!");
        ESP_LOGE(TAG, "System halted. Please restart.");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Start FSM processing task
    if (!system_start_fsm_task()) {
        ESP_LOGE(TAG, "Failed to start FSM task!");
        ESP_LOGE(TAG, "System halted. Please restart.");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "=== System Ready ===");
    ESP_LOGI(TAG, "Waiting for commands from web interface...");

    // Main idle loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
