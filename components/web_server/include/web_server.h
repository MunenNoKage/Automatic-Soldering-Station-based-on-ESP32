/**
 * @file web_server.h
 * @brief HTTP server for web interface
 * 
 * Serves web interface files and handles API requests.
 * Manages WebSocket connections for real-time updates.
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "fsm_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Web server configuration
 */
typedef struct {
    uint16_t port;
    uint32_t max_uri_handlers;
    uint32_t max_resp_headers;
    bool enable_websocket;
} web_server_config_t;

/**
 * @brief Web server handle
 */
typedef struct web_server_handle_s* web_server_handle_t;

/**
 * @brief Initialize and start web server
 * 
 * @param config Web server configuration
 * @param fsm_handle FSM controller handle for posting events
 */
web_server_handle_t web_server_init(const web_server_config_t* config, fsm_controller_handle_t fsm_handle);

/**
 * @brief Stop and deinitialize web server
 */
void web_server_deinit(web_server_handle_t handle);

/**
 * @brief Send status update to connected clients
 */
void web_server_broadcast_status(web_server_handle_t handle, const char* json_status);

/**
 * @brief Check if server is running
 */
bool web_server_is_running(web_server_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
