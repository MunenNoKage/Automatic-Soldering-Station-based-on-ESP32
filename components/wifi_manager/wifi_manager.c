/**
 * @file wifi_manager.c
 * @brief Implementation of WiFi Access Point manager with captive portal
 * 
 * Configures and manages ESP32 as WiFi hotspot with DNS server for
 * captive portal functionality on Android, iOS, and Windows devices.
 */

#include "wifi_manager.h"
#include "dns_server.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <lwip/ip4_addr.h>

static const char *TAG = "WIFI_MANAGER";

/**
 * @brief Internal structure for WiFi manager handle
 */
struct wifi_manager_handle_s {
    esp_netif_t *netif_ap;
    wifi_config_t wifi_cfg;  // ESP-IDF's wifi_config_t
    uint8_t connected_clients;
    bool is_running;
    char ip_address[16];
};

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    wifi_manager_handle_t handle = (wifi_manager_handle_t)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
                ESP_LOGI(TAG, "Client connected: MAC " MACSTR ", AID %d",
                        MAC2STR(event->mac), event->aid);
                handle->connected_clients++;
                break;
            }
            
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
                ESP_LOGI(TAG, "Client disconnected: MAC " MACSTR ", AID %d",
                        MAC2STR(event->mac), event->aid);
                if (handle->connected_clients > 0) {
                    handle->connected_clients--;
                }
                break;
            }
            
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                handle->is_running = true;
                break;
                
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP stopped");
                handle->is_running = false;
                break;
                
            default:
                break;
        }
    }
}

/**
 * @brief Initialize WiFi manager and start AP
 */
wifi_manager_handle_t wifi_manager_init(const wifi_manager_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi Access Point...");
    ESP_LOGI(TAG, "SSID: %s, Channel: %d", config->ssid, config->channel);
    
    // Allocate handle
    wifi_manager_handle_t handle = (wifi_manager_handle_t)calloc(1, sizeof(struct wifi_manager_handle_s));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }
    
    handle->connected_clients = 0;
    handle->is_running = false;
    
    // Initialize TCP/IP stack
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        free(handle);
        return NULL;
    }
    
    // Create default event loop if not exists
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        free(handle);
        return NULL;
    }
    
    // Create default WiFi AP netif
    handle->netif_ap = esp_netif_create_default_wifi_ap();
    if (!handle->netif_ap) {
        ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
        free(handle);
        return NULL;
    }
    
    // Configure AP with class A IP address for captive portal compatibility
    // NOTE: Using standard 192.168.4.1 for maximum compatibility
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    // Stop DHCP server before changing IP
    esp_netif_dhcps_stop(handle->netif_ap);
    
    // Set the new IP configuration
    ret = esp_netif_set_ip_info(handle->netif_ap, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set IP info: %s", esp_err_to_name(ret));
    }
    
    // Restart DHCP server with new configuration
    ret = esp_netif_dhcps_start(handle->netif_ap);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start DHCP server: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "AP configured with IP: " IPSTR, IP2STR(&ip_info.ip));
    
    // Store IP address in handle
    snprintf(handle->ip_address, sizeof(handle->ip_address), IPSTR, IP2STR(&ip_info.ip));
    
    // Initialize WiFi with default config
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        esp_netif_destroy(handle->netif_ap);
        free(handle);
        return NULL;
    }
    
    // Register event handler
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                     &wifi_event_handler, handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
    }
    
    // Configure WiFi AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .channel = config->channel,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = config->max_connections,
            .beacon_interval = 100,
        },
    };
    
    // Copy SSID
    strncpy((char*)wifi_config.ap.ssid, config->ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(config->ssid);
    
    // Store WiFi config
    memcpy(&handle->wifi_cfg, &wifi_config, sizeof(wifi_config_t));
    
    // Set WiFi mode to AP
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        esp_netif_destroy(handle->netif_ap);
        free(handle);
        return NULL;
    }
    
    // Set WiFi configuration
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        esp_netif_destroy(handle->netif_ap);
        free(handle);
        return NULL;
    }
    
    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        esp_netif_destroy(handle->netif_ap);
        free(handle);
        return NULL;
    }
    
    // Initialize DNS server for captive portal
    ESP_LOGI(TAG, "Starting DNS server for captive portal...");
    dns_server_init();
    
    ESP_LOGI(TAG, "WiFi AP initialized successfully");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SSID: %s", config->ssid);
    ESP_LOGI(TAG, "  Security: OPEN (No Password)");
    ESP_LOGI(TAG, "  IP Address: %s", handle->ip_address);
    ESP_LOGI(TAG, "  Channel: %d", config->channel);
    ESP_LOGI(TAG, "  Max Connections: %d", config->max_connections);
    ESP_LOGI(TAG, "  Captive Portal: ENABLED");
    ESP_LOGI(TAG, "========================================");
    
    return handle;
}
/**
 * @brief Deinitialize WiFi manager
 */
void wifi_manager_deinit(wifi_manager_handle_t handle) {
    if (!handle) {
        ESP_LOGW(TAG, "Invalid handle parameter");
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing WiFi manager");
    
    // Stop DNS server
    dns_server_deinit();
    
    // Unregister event handler
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    // Stop WiFi
    esp_wifi_stop();
    
    // Deinitialize WiFi
    esp_wifi_deinit();
    
    // Destroy netif
    if (handle->netif_ap) {
        esp_netif_destroy(handle->netif_ap);
        handle->netif_ap = NULL;
    }
    
    // Free handle
    free(handle);
    
    ESP_LOGI(TAG, "WiFi manager deinitialized");
}

/**
 * @brief Get number of connected clients
 */
uint8_t wifi_manager_get_client_count(wifi_manager_handle_t handle) {
    if (!handle) {
        ESP_LOGW(TAG, "Invalid handle parameter");
        return 0;
    }
    
    return handle->connected_clients;
}

/**
 * @brief Get AP IP address
 */
const char* wifi_manager_get_ip_address(wifi_manager_handle_t handle) {
    if (!handle) {
        ESP_LOGW(TAG, "Invalid handle parameter");
        return "0.0.0.0";
    }
    
    return handle->ip_address;
}

/**
 * @brief Check if AP is running
 */
bool wifi_manager_is_running(wifi_manager_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    return handle->is_running;
}
