/**
 * @file web_server.cpp
 * @brief Implementation of HTTP web server
 *
 * Serves web interface and handles API requests.
 */

#include "web_server.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "gcode_parser.h"
#include "execution_fsm.h"
#include "StepperMotor.hpp"

static const char *TAG = "WEB_SERVER";

// External motor objects (defined in main.cpp)
extern StepperMotor* motor_x;
extern StepperMotor* motor_y;
extern StepperMotor* motor_z;

// External GCode RAM buffer (defined in main.cpp)
extern char* g_gcode_buffer;
extern size_t g_gcode_size;
extern bool g_gcode_loaded;
extern SemaphoreHandle_t g_gcode_mutex;

// External execution sub-FSM (defined in main.cpp)
extern execution_sub_fsm_t exec_sub_fsm;

// Declare embedded files (created by CMake EMBED_FILES)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

extern const uint8_t manual_html_start[] asm("_binary_manual_html_start");
extern const uint8_t manual_html_end[] asm("_binary_manual_html_end");

extern const uint8_t exec_html_start[] asm("_binary_exec_html_start");
extern const uint8_t exec_html_end[] asm("_binary_exec_html_end");

extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

extern const uint8_t manual_js_start[] asm("_binary_manual_js_start");
extern const uint8_t manual_js_end[] asm("_binary_manual_js_end");

extern const uint8_t exec_js_start[] asm("_binary_exec_js_start");
extern const uint8_t exec_js_end[] asm("_binary_exec_js_end");

extern const uint8_t board_visualization_js_start[] asm("_binary_board_visualization_js_start");
extern const uint8_t board_visualization_js_end[] asm("_binary_board_visualization_js_end");

extern const uint8_t board_canvas_js_start[] asm("_binary_board_canvas_js_start");
extern const uint8_t board_canvas_js_end[] asm("_binary_board_canvas_js_end");

extern const uint8_t machine_canvas_js_start[] asm("_binary_machine_canvas_js_start");
extern const uint8_t machine_canvas_js_end[] asm("_binary_machine_canvas_js_end");

/**
 * @brief Structure for embedded file mapping
 */
typedef struct {
    const char* uri;
    const uint8_t* data_start;
    const uint8_t* data_end;
    const char* content_type;
} embedded_file_t;

/**
 * @brief Internal structure for web server handle
 */
struct web_server_handle_s {
    httpd_handle_t httpd_handle;
    web_server_config_t config;
    fsm_controller_handle_t fsm_handle;
};

/**
 * @brief Get embedded file data
 */
static const embedded_file_t* get_embedded_file(const char* uri) {
    static const embedded_file_t embedded_files[] = {
        {"/", index_html_start, index_html_end, "text/html"},
        {"/index.html", index_html_start, index_html_end, "text/html"},
        {"/manual.html", manual_html_start, manual_html_end, "text/html"},
        {"/exec.html", exec_html_start, exec_html_end, "text/html"},
        {"/style.css", style_css_start, style_css_end, "text/css"},
        {"/app.js", app_js_start, app_js_end, "application/javascript"},
        {"/manual.js", manual_js_start, manual_js_end, "application/javascript"},
        {"/exec.js", exec_js_start, exec_js_end, "application/javascript"},
        {"/board_visualization.js", board_visualization_js_start, board_visualization_js_end, "application/javascript"},
        {"/board_canvas.js", board_canvas_js_start, board_canvas_js_end, "application/javascript"},
        {"/machine_canvas.js", machine_canvas_js_start, machine_canvas_js_end, "application/javascript"},
    };

    for (size_t i = 0; i < sizeof(embedded_files) / sizeof(embedded_files[0]); i++) {
        if (strcmp(uri, embedded_files[i].uri) == 0) {
            return &embedded_files[i];
        }
    }

    return NULL;
}

/**
 * @brief Handler for serving static files
 */
static esp_err_t static_file_handler(httpd_req_t *req) {
    const char* uri = req->uri;

    ESP_LOGI(TAG, "Request for: %s", uri);

    // Get embedded file
    const embedded_file_t* file = get_embedded_file(uri);

    if (!file) {
        ESP_LOGW(TAG, "File not found: %s", uri);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Calculate file size
    size_t file_size = file->data_end - file->data_start;

    ESP_LOGI(TAG, "Serving embedded file: %s (%d bytes)", uri, file_size);

    // Set content type
    httpd_resp_set_type(req, file->content_type);

    // Send large files in chunks to avoid buffer overflow
    const size_t chunk_size = 4096;  // 4KB chunks
    if (file_size > chunk_size) {
        size_t offset = 0;
        while (offset < file_size) {
            size_t remaining = file_size - offset;
            size_t to_send = (remaining > chunk_size) ? chunk_size : remaining;

            esp_err_t err = httpd_resp_send_chunk(req, (const char*)(file->data_start + offset), to_send);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send chunk at offset %d: %s", offset, esp_err_to_name(err));
                return err;
            }

            offset += to_send;
            vTaskDelay(pdMS_TO_TICKS(1));  // Small delay to let WiFi task process
        }
        // Send empty chunk to signal end
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        // Small files can be sent at once
        httpd_resp_send(req, (const char*)file->data_start, file_size);
    }

    return ESP_OK;
}

/**
 * @brief Handler for API status endpoint
 */
static esp_err_t api_status_handler(httpd_req_t *req) {
    const char* status_json = "{\"status\":\"ok\",\"version\":\"1.0.0\",\"uptime\":0}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, status_json, strlen(status_json));

    return ESP_OK;
}

/**
 * @brief Handler for status streaming endpoint (polling alternative to WebSocket)
 */
static esp_err_t stream_status_handler(httpd_req_t *req) {
    // Simple endpoint for status polling
    // In a full implementation, this could support SSE (Server-Sent Events)

    const char* status_json = "{\"connected\":true,\"status\":\"idle\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, status_json, strlen(status_json));

    return ESP_OK;
}

/**
 * @brief Handler for G-Code/Drill file upload
 */
static esp_err_t gcode_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "G-Code upload request received");

    // Get web server handle (contains FSM handle)
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    // Buffer for receiving data
    char* buf = NULL;
    size_t buf_len = req->content_len;

    if (buf_len <= 0) {
        ESP_LOGW(TAG, "No content in upload request");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
        return ESP_FAIL;
    }

    if (buf_len > 512 * 1024) {  // 512KB max file size
        ESP_LOGW(TAG, "File too large: %d bytes", buf_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large (max 512KB)");
        return ESP_FAIL;
    }

    buf = (char*)malloc(buf_len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for upload");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Receive the data
    int ret = httpd_req_recv(req, buf, buf_len);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive data: %d", ret);
        free(buf);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[buf_len] = '\0';

    ESP_LOGI(TAG, "Received %d bytes of G-Code data", ret);

    // Buffer now contains raw GCode
    const char* gcode_content = buf;
    size_t gcode_len = buf_len;

    ESP_LOGI(TAG, "GCode content: %d bytes", gcode_len);

    // Validate GCode using parser
    gcode_parser_handle_t parser = gcode_parser_init();
    if (!parser) {
        ESP_LOGE(TAG, "Failed to initialize GCode parser");
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Parser initialization failed");
        return ESP_FAIL;
    }

    bool valid = gcode_parser_load_program(parser, gcode_content, gcode_len);
    if (!valid) {
        ESP_LOGW(TAG, "GCode validation failed");
        gcode_parser_deinit(parser);
        free(buf);

        const char* error_response = "{\"success\":false,\"message\":\"Invalid GCode format\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }

    // Count valid commands for validation
    gcode_command_t cmd;
    int valid_commands = 0;
    while (gcode_parser_get_next_command(parser, &cmd)) {
        valid_commands++;
    }

    ESP_LOGI(TAG, "GCode validation successful: %d valid commands", valid_commands);
    gcode_parser_deinit(parser);

    // Acquire mutex before modifying global buffer (thread safety)
    if (!g_gcode_mutex || xSemaphoreTake(g_gcode_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire GCode mutex (timeout or mutex not initialized)");
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Buffer locked - try again");
        return ESP_FAIL;
    }

    // Store GCode in global RAM buffer (protected by mutex)
    if (g_gcode_buffer) {
        free(g_gcode_buffer);  // Free old buffer if exists
        ESP_LOGI(TAG, "Freed previous GCode buffer");
    }

    g_gcode_buffer = (char*)malloc(gcode_len + 1);
    if (!g_gcode_buffer) {
        ESP_LOGE(TAG, "Failed to allocate RAM for GCode (%d bytes)", gcode_len);
        xSemaphoreGive(g_gcode_mutex);  // Release mutex before returning
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate RAM");
        return ESP_FAIL;
    }

    memcpy(g_gcode_buffer, gcode_content, gcode_len);
    g_gcode_buffer[gcode_len] = '\0';
    g_gcode_size = gcode_len;
    g_gcode_loaded = true;

    xSemaphoreGive(g_gcode_mutex);  // Release mutex after buffer update
    ESP_LOGI(TAG, "GCode stored in RAM: %d bytes (mutex released)", gcode_len);

    // Post FSM event: Task sent
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_TASK_SENT)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_TASK_SENT to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_TASK_SENT");
        }
    }

    // Send success response
    char response_buf[256];
    snprintf(response_buf, sizeof(response_buf),
             "{\"success\":true,\"message\":\"GCode uploaded and validated\","
             "\"size\":%d,\"commands\":%d,\"event_posted\":%s}",
             gcode_len, valid_commands, event_posted ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response_buf, strlen(response_buf));

    free(buf);
    return ESP_OK;
}

/**
 * @brief Handler for motor control commands
 */
static esp_err_t motor_control_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Motor control request received");

    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Motor command: %s", buf);

    // TODO: Parse motor command JSON
    // TODO: Execute motor movement
    // TODO: Return current position/status

    const char* response = "{\"success\":true,\"message\":\"Motor command received\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for getting motor status
 */
static esp_err_t motor_status_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Motor status request received");

    // TODO: Get actual motor positions and status
    const char* status_json = "{"
        "\"x_pos\":0.0,"
        "\"y_pos\":0.0,"
        "\"z_pos\":0.0,"
        "\"status\":\"idle\","
        "\"temperature\":25.0"
    "}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, status_json, strlen(status_json));

    return ESP_OK;
}

/**
 * @brief Handler for starting G-Code execution
 */
static esp_err_t gcode_start_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "G-Code start request received");

    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_TASK_APPROVED)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_TASK_APPROVED to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_TASK_APPROVED");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"G-Code execution started\"}"
        : "{\"success\":false,\"message\":\"Failed to start execution\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for stopping G-Code execution
 */
static esp_err_t gcode_stop_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "G-Code stop request received");

    // Get web server handle (contains FSM handle)
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    // Post FSM event: Task approved (to start execution)
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_EXIT_REQUEST)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_EXIT_REQUEST to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_EXIT_REQUEST");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"G-Code execution stopped\"}"
        : "{\"success\":false,\"message\":\"Failed to stop execution\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for pausing G-Code execution
 */
static esp_err_t gcode_pause_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "G-Code pause request received");

    // Get web server handle (contains FSM handle)
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    // Post FSM event: Task approved (to start execution)
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_PAUSE_REQUEST)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_PAUSE_REQUEST to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_PAUSE_REQUEST");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"G-Code execution paused\"}"
        : "{\"success\":false,\"message\":\"Failed to pause execution\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for resuming G-Code execution
 */
static esp_err_t gcode_resume_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "G-Code resume request received");

    // TODO: Resume G-Code execution

    // Get web server handle (contains FSM handle)
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    // Post FSM event: Task approved (to start execution)
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_CONTINUE_TASK)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_CONTINUE_TASK to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_CONTINUE_TASK");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"G-Code execution started\"}"
        : "{\"success\":false,\"message\":\"Failed to start execution\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for entering manual control mode
 */
static esp_err_t manual_enter_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Manual control mode entry request received");

    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_SELECT_MANUAL)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_SELECT_MANUAL to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_SELECT_MANUAL");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"Entering manual control mode\"}"
        : "{\"success\":false,\"message\":\"Failed to enter manual control mode\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for exiting manual control mode
 */
static esp_err_t manual_exit_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Manual control mode exit request received");

    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;

    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_EXIT_MANUAL)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_EXIT_MANUAL to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_EXIT_MANUAL");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"Exiting manual control mode\"}"
        : "{\"success\":false,\"message\":\"Failed to exit manual control mode\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for manual move command
 */
static esp_err_t manual_move_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Manual move request received");

    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    ret = (remaining < sizeof(buf) - 1) ? httpd_req_recv(req, buf, remaining) : httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON to extract X, Y, and Z coordinates
    // Simple parsing - expecting: {"x": 10.5, "y": 20.3, "z": 140.0}
    float x = 0.0f, y = 0.0f, z = 0.0f;
    bool has_z = false;
    char* x_pos = strstr(buf, "\"x\"");
    char* y_pos = strstr(buf, "\"y\"");
    char* z_pos = strstr(buf, "\"z\"");

    if (x_pos) {
        x_pos = strchr(x_pos, ':');
        if (x_pos) {
            x = atof(x_pos + 1);
        }
    }

    if (y_pos) {
        y_pos = strchr(y_pos, ':');
        if (y_pos) {
            y = atof(y_pos + 1);
        }
    }

    if (z_pos) {
        z_pos = strchr(z_pos, ':');
        if (z_pos) {
            z = atof(z_pos + 1);
            has_z = true;
        }
    }

    ESP_LOGI(TAG, "Manual move to X=%.2f, Y=%.2f%s%.2f", x, y, has_z ? ", Z=" : "", has_z ? z : 0.0f);

    // Generate G-code command for manual movement
    char gcode_cmd[64];
    if (has_z) {
        snprintf(gcode_cmd, sizeof(gcode_cmd), "G0 X%.2f Y%.2f Z%.2f", x, y, z);
    } else {
        snprintf(gcode_cmd, sizeof(gcode_cmd), "G0 X%.2f Y%.2f", x, y);
    }

    // Store in global G-code buffer (single command)
    if (xSemaphoreTake(g_gcode_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (g_gcode_buffer) {
            free(g_gcode_buffer);
        }
        g_gcode_buffer = strdup(gcode_cmd);
        g_gcode_size = strlen(gcode_cmd);
        g_gcode_loaded = true;
        xSemaphoreGive(g_gcode_mutex);

        ESP_LOGI(TAG, "Manual G-code stored: %s", gcode_cmd);
    } else {
        ESP_LOGE(TAG, "Failed to acquire G-code mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Mutex timeout");
        return ESP_FAIL;
    }

    // Post FSM event for manual command sent
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_MANUAL_COMMAND_SENT)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_MANUAL_COMMAND_SENT to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_MANUAL_COMMAND_SENT");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"Manual move command accepted\"}"
        : "{\"success\":false,\"message\":\"Failed to send move command\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for manual solder feed command
 */
static esp_err_t manual_solder_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Manual solder feed request received");

    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    ret = (remaining < sizeof(buf) - 1) ? httpd_req_recv(req, buf, remaining) : httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON to extract solder amount
    // Expecting: {"amount": 300}
    int amount = 300;  // Default
    char* amount_pos = strstr(buf, "\"amount\"");

    if (amount_pos) {
        amount_pos = strchr(amount_pos, ':');
        if (amount_pos) {
            amount = atoi(amount_pos + 1);
        }
    }

    // Validate amount
    if (amount < -1000 || amount > 1000) {
        ESP_LOGW(TAG, "Invalid solder amount: %d (must be -1000-1000)", amount);
        const char* error_response = "{\"success\":false,\"message\":\"Amount must be between 10 and 1000 steps\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Feeding solder: %d steps", amount);

    // Generate G-code command for solder feed
    char gcode_cmd[32];
    snprintf(gcode_cmd, sizeof(gcode_cmd), "S%d", amount);

    // Store in global G-code buffer (single command)
    if (xSemaphoreTake(g_gcode_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (g_gcode_buffer) {
            free(g_gcode_buffer);
        }
        g_gcode_buffer = strdup(gcode_cmd);
        g_gcode_size = strlen(gcode_cmd);
        g_gcode_loaded = true;
        xSemaphoreGive(g_gcode_mutex);

        ESP_LOGI(TAG, "Solder G-code stored: %s", gcode_cmd);
    } else {
        ESP_LOGE(TAG, "Failed to acquire G-code mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Mutex timeout");
        return ESP_FAIL;
    }

    // Post FSM event for manual command sent
    web_server_handle_t server_handle = (web_server_handle_t)req->user_ctx;
    bool event_posted = false;
    if (server_handle && server_handle->fsm_handle) {
        if (fsm_controller_post_event(server_handle->fsm_handle, FSM_EVENT_MANUAL_COMMAND_SENT)) {
            ESP_LOGI(TAG, "Posted FSM_EVENT_MANUAL_COMMAND_SENT to FSM controller");
            event_posted = true;
        } else {
            ESP_LOGW(TAG, "Failed to post FSM_EVENT_MANUAL_COMMAND_SENT");
        }
    }

    const char* response = event_posted
        ? "{\"success\":true,\"message\":\"Solder feed command accepted\"}"
        : "{\"success\":false,\"message\":\"Failed to send solder feed command\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief Handler for setting origin coordinates
 */
static esp_err_t manual_set_origin_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Set origin request received");

    // Get current motor positions in millimeters
    double x_origin = 0.0;
    double y_origin = 0.0;

    if (motor_x && motor_x->isInitialized()) {
        x_origin = motor_x->microsteps_to_mm(motor_x->getPosition());
    }

    if (motor_y && motor_y->isInitialized()) {
        y_origin = motor_y->microsteps_to_mm(motor_y->getPosition());
    }

    ESP_LOGI(TAG, "Setting origin to current position: X=%.2f mm, Y=%.2f mm", x_origin, y_origin);

    // Set origin in execution sub-FSM
    exec_sub_fsm_set_origin(&exec_sub_fsm, x_origin, y_origin);

    // Format JSON response
    char json[256];
    snprintf(json, sizeof(json),
             "{\"success\":true,\"message\":\"Origin set to current position\",\"x_origin\":%.2f,\"y_origin\":%.2f}",
             x_origin, y_origin);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

/**
 * @brief Handler for position status endpoint
 */
static esp_err_t position_status_handler(httpd_req_t *req) {
    // Get current motor positions in microsteps and convert to millimeters
    float x_mm = 0.0f;
    float y_mm = 0.0f;
    float z_mm = 0.0f;

    if (motor_x && motor_x->isInitialized()) {
        x_mm = motor_x->microsteps_to_mm(motor_x->getPosition());
    }

    if (motor_y && motor_y->isInitialized()) {
        y_mm = motor_y->microsteps_to_mm(motor_y->getPosition());
    }

    if (motor_z && motor_z->isInitialized()) {
        z_mm = motor_z->microsteps_to_mm(motor_z->getPosition());
    }

    // Format JSON response
    char json[128];
    snprintf(json, sizeof(json), "{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}", x_mm, y_mm, z_mm);

    ESP_LOGD(TAG, "Position: X=%.2f mm, Y=%.2f mm, Z=%.2f mm", x_mm, y_mm, z_mm);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/**
 * @brief Handler for OPTIONS requests (CORS preflight)
 */
static esp_err_t options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Initialize and start web server
 */
web_server_handle_t web_server_init(const web_server_config_t* config, fsm_controller_handle_t fsm_handle) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return NULL;
    }

    ESP_LOGI(TAG, "Initializing web server on port %d", config->port);

    // Allocate handle
    web_server_handle_t handle = (web_server_handle_t)calloc(1, sizeof(struct web_server_handle_s));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return NULL;
    }

    // Copy configuration and FSM handle
    memcpy(&handle->config, config, sizeof(web_server_config_t));
    handle->fsm_handle = fsm_handle;

    // Log embedded file sizes
    ESP_LOGI(TAG, "Embedded files:");
    ESP_LOGI(TAG, "  index.html: %d bytes", index_html_end - index_html_start);
    ESP_LOGI(TAG, "  style.css: %d bytes", style_css_end - style_css_start);
    ESP_LOGI(TAG, "  app.js: %d bytes", app_js_end - app_js_start);

    // Configure HTTP server
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = config->port;
    httpd_config.max_uri_handlers = config->max_uri_handlers;
    httpd_config.max_resp_headers = config->max_resp_headers;
    httpd_config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_config.stack_size = 6144;  // 6KB - optimal for ESP32 with chunked file serving
    httpd_config.task_priority = 5;  // Higher priority for responsiveness
    httpd_config.core_id = tskNO_AFFINITY;  // Allow running on any core
    httpd_config.recv_wait_timeout = 10;  // 10 second recv timeout
    httpd_config.send_wait_timeout = 10;  // 10 second send timeout
    httpd_config.lru_purge_enable = true;  // Enable LRU purge for multiple connections

    // Start HTTP server
    esp_err_t ret = httpd_start(&handle->httpd_handle, &httpd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        free(handle);
        return NULL;
    }

    // Register URI handlers

    // API status endpoint
    httpd_uri_t api_status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &api_status_uri);

    // G-Code upload endpoint
    httpd_uri_t gcode_upload_uri = {
        .uri = "/api/gcode/upload",
        .method = HTTP_POST,
        .handler = gcode_upload_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &gcode_upload_uri);

    // G-Code control endpoints
    httpd_uri_t gcode_start_uri = {
        .uri = "/api/gcode/start",
        .method = HTTP_POST,
        .handler = gcode_start_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &gcode_start_uri);

    httpd_uri_t gcode_stop_uri = {
        .uri = "/api/gcode/stop",
        .method = HTTP_POST,
        .handler = gcode_stop_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &gcode_stop_uri);

    httpd_uri_t gcode_pause_uri = {
        .uri = "/api/gcode/pause",
        .method = HTTP_POST,
        .handler = gcode_pause_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &gcode_pause_uri);

    httpd_uri_t gcode_resume_uri = {
        .uri = "/api/gcode/resume",
        .method = HTTP_POST,
        .handler = gcode_resume_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &gcode_resume_uri);

    // Manual control endpoints
    httpd_uri_t manual_enter_uri = {
        .uri = "/api/manual/enter",
        .method = HTTP_POST,
        .handler = manual_enter_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &manual_enter_uri);

    httpd_uri_t manual_exit_uri = {
        .uri = "/api/manual/exit",
        .method = HTTP_POST,
        .handler = manual_exit_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &manual_exit_uri);

    httpd_uri_t manual_move_uri = {
        .uri = "/api/manual/move",
        .method = HTTP_POST,
        .handler = manual_move_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &manual_move_uri);

    httpd_uri_t manual_set_origin_uri = {
        .uri = "/api/manual/set_origin",
        .method = HTTP_POST,
        .handler = manual_set_origin_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &manual_set_origin_uri);

    // Manual solder feed endpoint
    httpd_uri_t manual_solder_uri = {
        .uri = "/api/manual/solder",
        .method = HTTP_POST,
        .handler = manual_solder_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &manual_solder_uri);

    // Motor control endpoints
    httpd_uri_t motor_control_uri = {
        .uri = "/api/motor/move",
        .method = HTTP_POST,
        .handler = motor_control_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &motor_control_uri);

    httpd_uri_t motor_status_uri = {
        .uri = "/api/motor/status",
        .method = HTTP_GET,
        .handler = motor_status_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &motor_status_uri);

    // Position status endpoint
    httpd_uri_t position_status_uri = {
        .uri = "/api/status/position",
        .method = HTTP_GET,
        .handler = position_status_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &position_status_uri);

    // CORS preflight handler
    httpd_uri_t options_uri = {
        .uri = "/*",
        .method = HTTP_OPTIONS,
        .handler = options_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &options_uri);

    // Status streaming endpoint (polling alternative)
    if (config->enable_websocket) {
        httpd_uri_t stream_uri = {
            .uri = "/api/stream",
            .method = HTTP_GET,
            .handler = stream_status_handler,
            .user_ctx = handle
        };
        httpd_register_uri_handler(handle->httpd_handle, &stream_uri);
        ESP_LOGI(TAG, "Status streaming endpoint enabled");
    }

    // Static file handler (wildcard - must be last)
    httpd_uri_t static_file_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = handle
    };
    httpd_register_uri_handler(handle->httpd_handle, &static_file_uri);

    ESP_LOGI(TAG, "Web server started successfully on port %d", config->port);
    ESP_LOGI(TAG, "Registered API endpoints:");
    ESP_LOGI(TAG, "  GET  /api/status");
    ESP_LOGI(TAG, "  GET  /api/stream");
    ESP_LOGI(TAG, "  POST /api/gcode/upload");
    ESP_LOGI(TAG, "  POST /api/gcode/start");
    ESP_LOGI(TAG, "  POST /api/gcode/stop");
    ESP_LOGI(TAG, "  POST /api/gcode/pause");
    ESP_LOGI(TAG, "  POST /api/gcode/resume");
    ESP_LOGI(TAG, "  POST /api/manual/enter");
    ESP_LOGI(TAG, "  POST /api/manual/exit");
    ESP_LOGI(TAG, "  POST /api/manual/move");
    ESP_LOGI(TAG, "  POST /api/manual/set_origin");
    ESP_LOGI(TAG, "  POST /api/motor/move");
    ESP_LOGI(TAG, "  GET  /api/motor/status");
    ESP_LOGI(TAG, "  GET  /api/status/position");

    return handle;
}

/**
 * @brief Stop and deinitialize web server
 */
void web_server_deinit(web_server_handle_t handle) {
    if (!handle) {
        ESP_LOGW(TAG, "Invalid handle parameter");
        return;
    }

    ESP_LOGI(TAG, "Stopping web server");

    // Stop HTTP server
    if (handle->httpd_handle) {
        httpd_stop(handle->httpd_handle);
        handle->httpd_handle = NULL;
    }

    // Free handle
    free(handle);

    ESP_LOGI(TAG, "Web server stopped");
}

/**
 * @brief Send status update to connected clients
 */
void web_server_broadcast_status(web_server_handle_t handle, const char* json_status) {
    if (!handle || !json_status) {
        ESP_LOGW(TAG, "Invalid parameters for broadcast");
        return;
    }

    // Note: This is a simplified implementation without WebSocket support.
    // In a full implementation, you could:
    // 1. Store the status in a shared buffer
    // 2. Use Server-Sent Events (SSE)
    // 3. Enable WebSocket support if available in your ESP-IDF version

    ESP_LOGD(TAG, "Status update available: %s", json_status);

    // For now, just log the status. Clients can poll /api/stream endpoint
    // to retrieve the latest status.
}

/**
 * @brief Check if server is running
 */
bool web_server_is_running(web_server_handle_t handle) {
    return (handle && handle->httpd_handle != NULL);
}
