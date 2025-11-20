/**
 * @file gcode_parser.c
 * @brief Implementation of G-Code parser
 *
 * Parses G-Code commands for soldering station control.
 */

#include "gcode_parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <esp_log.h>

static const char *TAG = "GCODE_PARSER";

/**
 * @brief Internal structure for G-Code parser handle
 */
struct gcode_parser_handle_s {
    char* program_buffer;        // GCode program buffer
    size_t buffer_size;          // Total buffer size
    size_t current_position;     // Current position in buffer
    uint32_t current_line;       // Current line number
    bool is_loaded;              // Is program loaded
};

/**
 * @brief Initialize G-Code parser
 */
gcode_parser_handle_t gcode_parser_init(void) {
    gcode_parser_handle_t handle = (gcode_parser_handle_t)malloc(sizeof(struct gcode_parser_handle_s));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate parser handle");
        return NULL;
    }

    memset(handle, 0, sizeof(struct gcode_parser_handle_s));
    ESP_LOGI(TAG, "GCode parser initialized");
    return handle;
}

/**
 * @brief Deinitialize G-Code parser
 */
void gcode_parser_deinit(gcode_parser_handle_t handle) {
    if (!handle) return;

    if (handle->program_buffer) {
        free(handle->program_buffer);
    }
    free(handle);
    ESP_LOGI(TAG, "GCode parser deinitialized");
}

/**
 * @brief Skip whitespace in string
 */
static const char* skip_whitespace(const char* str) {
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

/**
 * @brief Parse a single parameter (e.g., X10.5, F100)
 */
static bool parse_parameter(const char** line, char* param_char, double* value) {
    const char* ptr = *line;

    // Get parameter letter
    if (!isalpha((unsigned char)*ptr)) {
        return false;
    }
    *param_char = toupper(*ptr);
    ptr++;

    // Parse numeric value
    char* end;
    *value = strtod(ptr, &end);

    if (end == ptr) {
        // No number found
        return false;
    }

    *line = end;
    return true;
}

/**
 * @brief Parse single line of G-Code
 */
bool gcode_parser_parse_line(gcode_parser_handle_t handle, const char* line, gcode_command_t* cmd) {
    if (!handle || !line || !cmd) {
        return false;
    }

    // Initialize command
    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->type = GCODE_CMD_NONE;

    // Skip leading whitespace
    line = skip_whitespace(line);

    // Skip empty lines and comments
    if (*line == '\0' || *line == ';' || *line == '\n' || *line == '\r') {
        return false;
    }

    // Parse command type (only G0 and S commands are supported)
    if (toupper(*line) == 'G') {
        line++;
        int g_code = atoi(line);

        switch (g_code) {
            case 0:
                // G0 - Rapid positioning (move)
                cmd->type = GCODE_CMD_MOVE;
                break;
            case 1:
            case 4:
            case 28:
                // G1 (linear move), G4 (dwell), G28 (home) are ignored
                // These operations are handled by the system
                ESP_LOGD(TAG, "Ignoring G-code G%d (handled by system)", g_code);
                return false;  // Skip this line
            default:
                ESP_LOGW(TAG, "Unsupported G-code: G%d (only G0 is supported)", g_code);
                return false;
        }

        // Skip the number
        while (isdigit((unsigned char)*line)) line++;
    }
    else if (toupper(*line) == 'M') {
        line++;
        int m_code = atoi(line);

        // All M-codes are ignored (system handles these)
        ESP_LOGD(TAG, "Ignoring M-code M%d (handled by system)", m_code);
        return false;  // Skip this line
    }
    else if (toupper(*line) == 'S') {
        // Custom command: S<amount> - Feed solder
        // Example: S75 means feed 75 units of solder
        cmd->type = GCODE_CMD_FEED_SOLDER;
        line++;

        // Parse solder amount
        if (isdigit((unsigned char)*line)) {
            cmd->has_s = true;
            cmd->s = (uint32_t)atoi(line);
            // Skip the number
            while (isdigit((unsigned char)*line)) line++;
        } else {
            // Default solder amount if no number provided
            cmd->has_s = true;
            cmd->s = 100;  // Default solder feed amount
        }
    }
    else {
        ESP_LOGW(TAG, "Invalid command format: %s", line);
        return false;
    }

    // Parse parameters
    while (*line) {
        line = skip_whitespace(line);

        // Check for comment
        if (*line == ';' || *line == '\0' || *line == '\n' || *line == '\r') {
            break;
        }

        char param_char;
        double value;

        if (parse_parameter(&line, &param_char, &value)) {
            switch (param_char) {
                case 'X':
                    cmd->has_x = true;
                    cmd->x = value;
                    break;
                case 'Y':
                    cmd->has_y = true;
                    cmd->y = value;
                    break;
                case 'Z':
                    // Z parameter is parsed but ignored (Z movement is system-configured)
                    cmd->has_z = true;
                    cmd->z = value;
                    ESP_LOGD(TAG, "Z parameter ignored (Z movement handled by system)");
                    break;
                case 'F':
                    // F (feed rate) parameter is parsed but ignored (feed rates are system-configured)
                    cmd->has_f = true;
                    cmd->f = value;
                    ESP_LOGD(TAG, "F parameter ignored (feed rates handled by system)");
                    break;
                case 'S':
                    cmd->has_s = true;
                    cmd->s = (uint32_t)value;
                    break;
                case 'T':
                case 'P':
                    // T and P parameters are ignored (timing is system-configured)
                    ESP_LOGD(TAG, "%c parameter ignored (timing handled by system)", param_char);
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown parameter: %c", param_char);
                    break;
            }
        } else {
            break;
        }
    }

    return (cmd->type != GCODE_CMD_NONE && cmd->type != GCODE_CMD_UNKNOWN);
}

/**
 * @brief Validate G-Code command
 */
bool gcode_parser_validate_command(gcode_parser_handle_t handle, const gcode_command_t* cmd) {
    if (!handle || !cmd) {
        return false;
    }

    // Basic validation - only G0 (move) and S (feed solder) are supported
    switch (cmd->type) {
        case GCODE_CMD_MOVE:
            // X and Y coordinates are required for G0 (Z is system-configured)
            if (!cmd->has_x || !cmd->has_y) {
                ESP_LOGW(TAG, "G0 move command requires X and Y coordinates");
                return false;
            }
            break;

        case GCODE_CMD_FEED_SOLDER:
            // Solder feed amount should be specified
            if (!cmd->has_s || cmd->s == 0) {
                ESP_LOGW(TAG, "Feed solder command without valid amount");
                return false;
            }
            break;

        case GCODE_CMD_HOME:
        case GCODE_CMD_DWELL:
        case GCODE_CMD_SET_TEMPERATURE:
            // These commands should not reach here (ignored during parsing)
            ESP_LOGW(TAG, "Command type %d should have been filtered during parsing", cmd->type);
            return false;

        default:
            ESP_LOGW(TAG, "Invalid command type: %d", cmd->type);
            return false;
    }

    return true;
}

/**
 * @brief Load G-Code from buffer
 */
bool gcode_parser_load_program(gcode_parser_handle_t handle, const char* program, uint32_t length) {
    if (!handle || !program || length == 0) {
        ESP_LOGE(TAG, "Invalid parameters for load_program");
        return false;
    }

    // Free existing buffer if any
    if (handle->program_buffer) {
        free(handle->program_buffer);
    }

    // Allocate new buffer
    handle->program_buffer = (char*)malloc(length + 1);
    if (!handle->program_buffer) {
        ESP_LOGE(TAG, "Failed to allocate program buffer (%d bytes)", length);
        return false;
    }

    // Copy program
    memcpy(handle->program_buffer, program, length);
    handle->program_buffer[length] = '\0';

    handle->buffer_size = length;
    handle->current_position = 0;
    handle->current_line = 0;
    handle->is_loaded = true;

    ESP_LOGI(TAG, "Loaded GCode program (%d bytes)", length);
    return true;
}

/**
 * @brief Get next command from loaded program
 */
bool gcode_parser_get_next_command(gcode_parser_handle_t handle, gcode_command_t* cmd) {
    if (!handle || !cmd || !handle->is_loaded) {
        return false;
    }

    // Check if we've reached the end
    if (handle->current_position >= handle->buffer_size) {
        ESP_LOGI(TAG, "End of program reached");
        return false;
    }

    // Extract next line
    char line_buffer[256];
    size_t line_length = 0;

    while (handle->current_position < handle->buffer_size && line_length < sizeof(line_buffer) - 1) {
        char c = handle->program_buffer[handle->current_position++];

        if (c == '\n' || c == '\r') {
            // End of line
            if (c == '\r' && handle->current_position < handle->buffer_size
                && handle->program_buffer[handle->current_position] == '\n') {
                // Skip \r\n
                handle->current_position++;
            }
            break;
        }

        line_buffer[line_length++] = c;
    }

    line_buffer[line_length] = '\0';
    handle->current_line++;

    // Parse the line
    if (gcode_parser_parse_line(handle, line_buffer, cmd)) {
        if (gcode_parser_validate_command(handle, cmd)) {
            ESP_LOGI(TAG, "Line %d: Parsed command type %d", handle->current_line, cmd->type);
            return true;
        } else {
            ESP_LOGW(TAG, "Line %d: Command validation failed", handle->current_line);
            // Try next line
            return gcode_parser_get_next_command(handle, cmd);
        }
    }

    // Empty line or comment, try next line
    if (handle->current_position < handle->buffer_size) {
        return gcode_parser_get_next_command(handle, cmd);
    }

    return false;
}

/**
 * @brief Reset parser to beginning of program
 */
void gcode_parser_reset(gcode_parser_handle_t handle) {
    if (!handle) return;

    handle->current_position = 0;
    handle->current_line = 0;
    ESP_LOGI(TAG, "Parser reset to beginning");
}

/**
 * @brief Get current line number
 */
uint32_t gcode_parser_get_line_number(gcode_parser_handle_t handle) {
    if (!handle) return 0;
    return handle->current_line;
}
