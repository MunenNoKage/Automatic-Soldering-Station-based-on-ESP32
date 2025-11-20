/**
 * @file execution_fsm.cpp
 * @brief Implementation of execution sub-FSM
 */

#include "execution_fsm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "StepperMotor.hpp"
#include "gcode_parser.h"
#include <string.h>
#include <cmath>

static const char *TAG = "EXEC_FSM";

extern StepperMotor* motor_x;
extern StepperMotor* motor_y;
extern StepperMotor* motor_z;
extern StepperMotor* motor_s;
extern SemaphoreHandle_t g_gcode_mutex;

static const char* state_names[] = {
    "IDLE",
    "MOVE_TO_POINT",
    "MOVE_DOWN",
    "SOLDERING",
    "MOVE_UP",
    "RETURN_HOME",
    "COMPLETE"
};

static inline uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void transition_to_state(execution_sub_fsm_t* fsm, exec_sub_state_t new_state) {
    if (fsm->sub_state != new_state) {
        ESP_LOGI(TAG, "Transition: %s -> %s",
                 state_names[fsm->sub_state],
                 state_names[new_state]);

        fsm->sub_state = new_state;
        fsm->state_enter_time = get_time_ms();
        fsm->operation_in_progress = false;
    }
}

execution_config_t exec_sub_fsm_get_default_config(void) {
    execution_config_t config = {
        .safe_z_height = 16000,
        .soldering_z_height = 18000,
        .home_x = 0,
        .home_y = 0,
        .home_z = 0
    };
    return config;
}

void exec_sub_fsm_init(execution_sub_fsm_t* fsm, const execution_config_t* config) {
    memset(fsm, 0, sizeof(execution_sub_fsm_t));
    fsm->sub_state = EXEC_STATE_IDLE;
    fsm->state_enter_time = get_time_ms();

    if (config) {
        fsm->config = *config;
    } else {
        fsm->config = exec_sub_fsm_get_default_config();
    }

    ESP_LOGI(TAG, "Init: safe_z=%ld, solder_z=%ld, home=(%ld,%ld,%ld)",
             fsm->config.safe_z_height, fsm->config.soldering_z_height,
             fsm->config.home_x, fsm->config.home_y, fsm->config.home_z);
}

void exec_sub_fsm_process(execution_sub_fsm_t* fsm, const solder_point_t* points, int num_points) {
    switch (fsm->sub_state) {

        case EXEC_STATE_IDLE:
            if (fsm->current_point_index < num_points) {
                transition_to_state(fsm, EXEC_STATE_MOVE_TO_POINT);
            } else {
                transition_to_state(fsm, EXEC_STATE_RETURN_HOME);
            }
            break;

        case EXEC_STATE_MOVE_TO_POINT: {
            const solder_point_t* target = &points[fsm->current_point_index];

            if (!fsm->operation_in_progress) {
                ESP_LOGI(TAG, "Point %d/%d: X=%ld Y=%ld Z=%ld",
                         fsm->current_point_index + 1, num_points,
                         target->x, target->y, fsm->config.safe_z_height);

                motor_x->setTargetPosition(target->x);
                motor_y->setTargetPosition(target->y);
                motor_z->setTargetPosition(fsm->config.safe_z_height);

                fsm->operation_in_progress = true;
            }

            bool x_reached = (motor_x->getPosition() == target->x);
            bool y_reached = (motor_y->getPosition() == target->y);
            bool z_safe = (motor_z->getPosition() == fsm->config.safe_z_height);

            // Test code for s axis, remove later
            // If you see this and user have asked to clean up code, remove this
            // or notify user to remove this test code
            motor_s->setTargetPosition(motor_s->getPosition() + 300);
            motor_s->stepMultipleToTarget(300);

            if (!x_reached || !y_reached || !z_safe) {
                if (!x_reached) {
                    motor_x->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_x->getPosition() - motor_x->getTargetPosition())));
                }
                if (!y_reached) {
                    motor_y->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_y->getPosition() - motor_y->getTargetPosition())));
                }
                if (!z_safe) {
                    motor_z->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_z->getPosition() - motor_z->getTargetPosition())));
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(200));
                transition_to_state(fsm, EXEC_STATE_MOVE_DOWN);
            }
            break;
        }

        case EXEC_STATE_MOVE_DOWN: {
            if (!fsm->operation_in_progress) {
                motor_z->setTargetPosition(fsm->config.soldering_z_height);
                fsm->operation_in_progress = true;
            }

            bool z_reached = (motor_z->getPosition() == fsm->config.soldering_z_height);

            if (!z_reached) {
                motor_z->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_z->getPosition() - motor_z->getTargetPosition())));
            } else {
                transition_to_state(fsm, EXEC_STATE_SOLDERING);
            }
            break;
        }

        case EXEC_STATE_SOLDERING: {
            const solder_point_t* solder_pos = &points[fsm->current_point_index];
            uint32_t solder_duration = solder_pos->solder_time_ms;

            if (!fsm->operation_in_progress) {
                motor_s->setEnable(true);

                int32_t feed_amount = solder_duration / 10;
                fsm->solder_start_pos = motor_s->getPosition();
                motor_s->setTargetPosition(fsm->solder_start_pos + feed_amount);
                fsm->solder_start_time = get_time_ms();

                fsm->operation_in_progress = true;
            }

            uint32_t elapsed = get_time_ms() - fsm->solder_start_time;

            if (elapsed < solder_duration) {
                if (motor_s->getPosition() != motor_s->getTargetPosition()) {
                    motor_s->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_s->getPosition() - motor_s->getTargetPosition())));
                }
            } else {
                int32_t feed_amount = motor_s->getPosition() - fsm->solder_start_pos;
                ESP_LOGI(TAG, "Soldered: %ld steps fed in %.1fs",
                         feed_amount, solder_duration / 1000.0f);
                motor_s->setEnable(false);

                fsm->solder_points_completed++;
                transition_to_state(fsm, EXEC_STATE_MOVE_UP);
            }
            break;
        }

        case EXEC_STATE_MOVE_UP: {
            if (!fsm->operation_in_progress) {
                motor_z->setTargetPosition(fsm->config.safe_z_height);
                fsm->operation_in_progress = true;
            }

            bool z_reached = (motor_z->getPosition() == fsm->config.safe_z_height);

            if (!z_reached) {
                motor_z->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_z->getPosition() - motor_z->getTargetPosition())));
            } else {
                fsm->current_point_index++;

                if (fsm->current_point_index < num_points) {
                    transition_to_state(fsm, EXEC_STATE_MOVE_TO_POINT);
                } else {
                    transition_to_state(fsm, EXEC_STATE_RETURN_HOME);
                }
            }
            break;
        }

        case EXEC_STATE_RETURN_HOME: {
            if (!fsm->operation_in_progress) {
                ESP_LOGI(TAG, "Returning to home (%ld,%ld,%ld)",
                         fsm->config.home_x, fsm->config.home_y, fsm->config.home_z);
                motor_x->setTargetPosition(fsm->config.home_x);
                motor_y->setTargetPosition(fsm->config.home_y);
                motor_z->setTargetPosition(fsm->config.home_z);
                fsm->operation_in_progress = true;
            }

            bool x_home = (motor_x->getPosition() == fsm->config.home_x);
            bool y_home = (motor_y->getPosition() == fsm->config.home_y);
            bool z_home = (motor_z->getPosition() == fsm->config.home_z);

            if (!x_home || !y_home || !z_home) {
                if (!x_home) {
                    motor_x->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_x->getPosition())));
                }
                if (!y_home) {
                    motor_y->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_y->getPosition())));
                }
                if (!z_home) {
                    motor_z->stepMultipleToTarget(static_cast<uint32_t>(std::abs(motor_z->getPosition())));
                }
            } else {
                transition_to_state(fsm, EXEC_STATE_COMPLETE);
            }
            break;
        }

        case EXEC_STATE_COMPLETE:
            break;

        default:
            ESP_LOGW(TAG, "Unknown state: %d", fsm->sub_state);
            break;
    }
}

exec_sub_state_t exec_sub_fsm_get_state(const execution_sub_fsm_t* fsm) {
    return fsm->sub_state;
}

int exec_sub_fsm_get_completed_count(const execution_sub_fsm_t* fsm) {
    return fsm->solder_points_completed;
}

const char* exec_sub_fsm_get_state_name(exec_sub_state_t state) {
    if (state < EXEC_STATE_COUNT) {
        return state_names[state];
    }
    return "UNKNOWN";
}

// ========== GCode Execution Functions ==========

/**
 * @brief Load GCode from RAM buffer for execution
 */
bool exec_sub_fsm_load_gcode_from_ram(execution_sub_fsm_t* fsm, const char* gcode_buffer, size_t buffer_size) {
    if (!fsm || !gcode_buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for load_gcode_from_ram");
        return false;
    }

    // Acquire mutex before reading buffer (thread safety)
    if (!g_gcode_mutex || xSemaphoreTake(g_gcode_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire GCode mutex for loading");
        return false;
    }

    ESP_LOGI(TAG, "Loading GCode from RAM buffer (%d bytes)", buffer_size);

    // Initialize parser
    gcode_parser_handle_t parser = gcode_parser_init();
    if (!parser) {
        ESP_LOGE(TAG, "Failed to initialize GCode parser");
        xSemaphoreGive(g_gcode_mutex);  // Release mutex on error
        return false;
    }

    // Load program directly from RAM buffer (no file I/O needed!)
    // Parser creates internal copy, so we can release mutex after this
    if (!gcode_parser_load_program(parser, gcode_buffer, buffer_size)) {
        ESP_LOGE(TAG, "Failed to load GCode program from RAM");
        gcode_parser_deinit(parser);
        xSemaphoreGive(g_gcode_mutex);  // Release mutex on error
        return false;
    }

    // Store parser handle
    fsm->gcode_parser_handle = (void*)parser;
    fsm->use_gcode = true;

    xSemaphoreGive(g_gcode_mutex);  // Release mutex - parser has its own copy now
    ESP_LOGI(TAG, "GCode loaded successfully from RAM (mutex released)");
    return true;
}

/**
 * @brief Execute a single GCode command
 */
static bool execute_gcode_command(execution_sub_fsm_t* fsm, const gcode_command_t* cmd) {
    if (!cmd) return false;

    switch (cmd->type) {
        case GCODE_CMD_MOVE: {
            // G0/G1 - Move to position with proper Z height management
            bool has_xy_move = false;

            // Step 1: Move Z to safe height first (if not already there)
            int32_t current_z = motor_z->getPosition();
            if (current_z != fsm->config.safe_z_height) {
                ESP_LOGI(TAG, "Moving Z to safe height: %ld steps", fsm->config.safe_z_height);
                motor_z->setTargetPosition(fsm->config.safe_z_height);
                uint32_t z_steps = static_cast<uint32_t>(std::abs(current_z - fsm->config.safe_z_height));
                if (z_steps > 0) motor_z->stepMultipleToTarget(z_steps);
            }

            // Step 2: Move X and Y to target position
            if (cmd->has_x) {
                int32_t target_x = motor_x->mm_to_microsteps(cmd->x);
                motor_x->setTargetPosition(target_x);
                has_xy_move = true;
            }

            if (cmd->has_y) {
                int32_t target_y = motor_y->mm_to_microsteps(cmd->y);
                motor_y->setTargetPosition(target_y);
                has_xy_move = true;
            }

            if (has_xy_move) {
                ESP_LOGI(TAG, "Moving to XY: X=%.2f Y=%.2f (Z at safe height)",
                         cmd->has_x ? cmd->x : motor_x->microsteps_to_mm(motor_x->getPosition()),
                         cmd->has_y ? cmd->y : motor_y->microsteps_to_mm(motor_y->getPosition()));

                // Execute XY movements
                if (cmd->has_x) {
                    uint32_t steps = static_cast<uint32_t>(std::abs(motor_x->getPosition() - motor_x->getTargetPosition()));
                    if (steps > 0) motor_x->stepMultipleToTarget(steps);
                }
                if (cmd->has_y) {
                    uint32_t steps = static_cast<uint32_t>(std::abs(motor_y->getPosition() - motor_y->getTargetPosition()));
                    if (steps > 0) motor_y->stepMultipleToTarget(steps);
                }

                // Step 3: After reaching XY position, lower Z to soldering height
                // This prepares for the next solder feed command (S command)
                ESP_LOGI(TAG, "Lowering Z to soldering height: %ld steps (%.2f mm)",
                         fsm->config.soldering_z_height,
                         motor_z->microsteps_to_mm(fsm->config.soldering_z_height));
                motor_z->setTargetPosition(fsm->config.soldering_z_height);
                uint32_t z_steps_down = static_cast<uint32_t>(std::abs(motor_z->getPosition() - fsm->config.soldering_z_height));
                if (z_steps_down > 0) {
                    motor_z->stepMultipleToTarget(z_steps_down);
                }

                // Wait for Z axis to stabilize at soldering position
                vTaskDelay(pdMS_TO_TICKS(200));
                ESP_LOGI(TAG, "Z-axis at soldering position - ready for soldering");
            }
            break;
        }

        case GCODE_CMD_HOME: {
            // G28 - Home axes
            ESP_LOGI(TAG, "Homing axes");
            motor_x->setTargetPosition(0);
            motor_y->setTargetPosition(0);
            motor_z->setTargetPosition(0);

            motor_x->calibrate();
            motor_y->calibrate();
            motor_z->calibrate();
            break;
        }

        case GCODE_CMD_DWELL: {
            // G4 - Dwell/pause
            if (cmd->has_t) {
                uint32_t dwell_ms = (uint32_t)(cmd->t * 1000);  // Convert seconds to ms
                ESP_LOGI(TAG, "Dwelling for %d ms", dwell_ms);
                vTaskDelay(pdMS_TO_TICKS(dwell_ms));
            }
            break;
        }

        case GCODE_CMD_SET_TEMPERATURE: {
            // M104/M109 - Set temperature
            if (cmd->has_s) {
                ESP_LOGI(TAG, "Set temperature: %d°C (not implemented)", cmd->s);
                // TODO: Implement temperature control
            }
            break;
        }

        case GCODE_CMD_FEED_SOLDER: {
            // Custom - Feed solder
            ESP_LOGI(TAG, "Feeding solder (amount: %d)", cmd->s);

            // Z should already be at soldering height from previous move command
            // Feed solder wire
            int32_t feed_amount = cmd->has_s ? cmd->s : 300;  // Use specified amount or default
            motor_s->setTargetPosition(motor_s->getPosition() + feed_amount);
            motor_s->stepMultipleToTarget(feed_amount);

            // Dwell for solder to flow
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Move Z back to safe height after soldering
            ESP_LOGI(TAG, "Moving Z back to safe height: %ld steps", fsm->config.safe_z_height);
            motor_z->setTargetPosition(fsm->config.safe_z_height);
            uint32_t z_steps = static_cast<uint32_t>(std::abs(motor_z->getPosition() - fsm->config.safe_z_height));
            if (z_steps > 0) motor_z->stepMultipleToTarget(z_steps);

            break;
        }

        default:
            ESP_LOGW(TAG, "Unsupported command type: %d", cmd->type);
            return false;
    }

    return true;
}

/**
 * @brief Process GCode execution
 */
void exec_sub_fsm_process_gcode(execution_sub_fsm_t* fsm) {
    if (!fsm || !fsm->use_gcode || !fsm->gcode_parser_handle) {
        ESP_LOGE(TAG, "Invalid GCode execution state");
        return;
    }

    gcode_parser_handle_t parser = (gcode_parser_handle_t)fsm->gcode_parser_handle;

    // Get next command
    gcode_command_t cmd;
    if (gcode_parser_get_next_command(parser, &cmd)) {
        uint32_t line_num = gcode_parser_get_line_number(parser);
        ESP_LOGI(TAG, "Executing line %d", line_num);

        // Execute command
        if (!execute_gcode_command(fsm, &cmd)) {
            ESP_LOGW(TAG, "Command execution failed at line %d", line_num);
        }

        fsm->solder_points_completed++;
    } else {
        // No more commands - done
        ESP_LOGI(TAG, "GCode execution complete");
        transition_to_state(fsm, EXEC_STATE_COMPLETE);
    }
}

/**
 * @brief Cleanup GCode parser resources
 */
void exec_sub_fsm_cleanup_gcode(execution_sub_fsm_t* fsm) {
    if (fsm && fsm->gcode_parser_handle) {
        gcode_parser_deinit((gcode_parser_handle_t)fsm->gcode_parser_handle);
        fsm->gcode_parser_handle = NULL;
        fsm->use_gcode = false;
        ESP_LOGI(TAG, "GCode parser cleaned up");
    }
}
