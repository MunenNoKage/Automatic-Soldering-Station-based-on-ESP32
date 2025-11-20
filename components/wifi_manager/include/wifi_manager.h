/**
 * @file wifi_manager.h
 * @brief WiFi Access Point manager for hosting web interface
 * 
 * Manages ESP32 WiFi hotspot for client connections.
 * Provides network configuration and status monitoring.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    const char* ssid;
    uint8_t channel;
    uint8_t max_connections;
} wifi_manager_config_t;

/**
 * @brief WiFi manager handle
 */
typedef struct wifi_manager_handle_s* wifi_manager_handle_t;

/**
 * @brief Initialize WiFi manager and start AP
 */
wifi_manager_handle_t wifi_manager_init(const wifi_manager_config_t* config);

/**
 * @brief Deinitialize WiFi manager
 */
void wifi_manager_deinit(wifi_manager_handle_t handle);

/**
 * @brief Get number of connected clients
 */
uint8_t wifi_manager_get_client_count(wifi_manager_handle_t handle);

/**
 * @brief Get AP IP address
 */
const char* wifi_manager_get_ip_address(wifi_manager_handle_t handle);

/**
 * @brief Check if AP is running
 */
bool wifi_manager_is_running(wifi_manager_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
