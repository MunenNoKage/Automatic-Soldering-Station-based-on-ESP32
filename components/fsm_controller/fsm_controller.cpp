/**
 * @file fsm_controller.cpp
 * @brief Implementation of FSM controller for automatic soldering station
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2025
 */

#include "fsm_controller.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "FSM_CONTROLLER";

/**
 * @brief State transition definition
 */
typedef struct {
    fsm_state_t from_state;
    fsm_event_t event;
    fsm_state_t to_state;
} state_transition_t;

/**
 * @brief State callback set
 */
typedef struct {
    fsm_state_enter_callback_t on_enter;
    fsm_state_exit_callback_t on_exit;
    fsm_state_execute_callback_t on_execute;
    void* enter_user_data;
    void* exit_user_data;
    void* execute_user_data;
} state_callbacks_t;

/**
 * @brief FSM Controller internal structure
 */
struct fsm_controller_s {
    fsm_config_t config;
    fsm_state_t current_state;
    fsm_state_t previous_state;
    bool is_running;
    uint32_t state_enter_time;

    // Event queue
    QueueHandle_t event_queue;

    // Callbacks for each state
    state_callbacks_t callbacks[FSM_STATE_COUNT];

    // Statistics
    fsm_statistics_t statistics;

    // Execution context for current state
    fsm_execution_context_t exec_context;
};

/**
 * @brief State transition table
 * Defines all valid state transitions
 */
static const state_transition_t state_transitions[] = {
    // From INIT
    {FSM_STATE_INIT, FSM_EVENT_INIT_DONE, FSM_STATE_IDLE},

    // From IDLE
    {FSM_STATE_IDLE, FSM_EVENT_SELECT_MANUAL, FSM_STATE_MANUAL_CONTROL},
    {FSM_STATE_IDLE, FSM_EVENT_TASK_SENT, FSM_STATE_CALIBRATION},
    {FSM_STATE_IDLE, FSM_EVENT_REQUEST_CALIBRATION, FSM_STATE_CALIBRATION},

    // From MANUAL_CONTROL
    {FSM_STATE_MANUAL_CONTROL, FSM_EVENT_EXIT_MANUAL, FSM_STATE_IDLE},

    // From CALIBRATION
    {FSM_STATE_CALIBRATION, FSM_EVENT_CALIBRATION_SUCCESS, FSM_STATE_READY},
    {FSM_STATE_CALIBRATION, FSM_EVENT_CALIBRATION_ERROR, FSM_STATE_CALIBRATION_ERROR},

    // From READY
    {FSM_STATE_READY, FSM_EVENT_CANCEL_TASK, FSM_STATE_IDLE},
    {FSM_STATE_READY, FSM_EVENT_CALIBRATION_DONE, FSM_STATE_IDLE},
    {FSM_STATE_READY, FSM_EVENT_TASK_APPROVED, FSM_STATE_HEATING},

    // From HEATING
    {FSM_STATE_HEATING, FSM_EVENT_HEATING_SUCCESS, FSM_STATE_EXECUTING},
    {FSM_STATE_HEATING, FSM_EVENT_HEATING_ERROR, FSM_STATE_HEATING_ERROR},

    // From EXECUTING
    {FSM_STATE_EXECUTING, FSM_EVENT_PAUSE_REQUEST, FSM_STATE_PAUSED},
    {FSM_STATE_EXECUTING, FSM_EVENT_TASK_DONE, FSM_STATE_NORMAL_EXIT},
    {FSM_STATE_EXECUTING, FSM_EVENT_HEATING_ERROR, FSM_STATE_HEATING_ERROR},
    {FSM_STATE_EXECUTING, FSM_EVENT_DATA_ERROR, FSM_STATE_DATA_ERROR},

    // From PAUSED
    {FSM_STATE_PAUSED, FSM_EVENT_EXIT_REQUEST, FSM_STATE_NORMAL_EXIT},
    {FSM_STATE_PAUSED, FSM_EVENT_CONTINUE_TASK, FSM_STATE_HEATING},

    // From NORMAL_EXIT
    {FSM_STATE_NORMAL_EXIT, FSM_EVENT_COOLDOWN_COMPLETE, FSM_STATE_IDLE},
    {FSM_STATE_NORMAL_EXIT, FSM_EVENT_COOLING_ERROR, FSM_STATE_HEATING_ERROR},

    // From ERROR states to LOCK
    {FSM_STATE_CALIBRATION_ERROR, FSM_EVENT_CALIBRATION_ERROR, FSM_STATE_LOCK},
    {FSM_STATE_HEATING_ERROR, FSM_EVENT_HEATING_ERROR, FSM_STATE_LOCK},
    {FSM_STATE_DATA_ERROR, FSM_EVENT_DATA_ERROR, FSM_STATE_LOCK},
};

static const size_t NUM_TRANSITIONS = sizeof(state_transitions) / sizeof(state_transitions[0]);

/**
 * @brief State names for logging and debugging
 */
static const char* state_names[] = {
    "INIT",
    "IDLE",
    "MANUAL_CONTROL",
    "CALIBRATION",
    "READY",
    "HEATING",
    "EXECUTING",
    "PAUSED",
    "NORMAL_EXIT",
    "CALIBRATION_ERROR",
    "HEATING_ERROR",
    "DATA_ERROR",
    "LOCK"
};

/**
 * @brief Event names for logging and debugging
 */
static const char* event_names[] = {
    "INIT_DONE",
    "SELECT_MANUAL",
    "EXIT_MANUAL",
    "TASK_SENT",
    "REQUEST_CALIBRATION",
    "CALIBRATION_SUCCESS",
    "CALIBRATION_ERROR",
    "CANCEL_TASK",
    "CALIBRATION_DONE",
    "TASK_APPROVED",
    "HEATING_SUCCESS",
    "HEATING_ERROR",
    "PAUSE_REQUEST",
    "TASK_DONE",
    "DATA_ERROR",
    "EXIT_REQUEST",
    "CONTINUE_TASK",
    "COOLDOWN_COMPLETE",
    "COOLING_ERROR"
};

// Forward declarations
static bool transition_to_state(fsm_controller_handle_t handle, fsm_state_t new_state);
static fsm_state_t find_next_state(fsm_state_t current_state, fsm_event_t event);

/**
 * @brief Get current time in milliseconds
 */
static uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Initialize FSM controller
 */
fsm_controller_handle_t fsm_controller_init(const fsm_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return NULL;
    }

    fsm_controller_handle_t handle = (fsm_controller_handle_t)malloc(sizeof(struct fsm_controller_s));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate FSM controller");
        return NULL;
    }

    memset(handle, 0, sizeof(struct fsm_controller_s));
    memcpy(&handle->config, config, sizeof(fsm_config_t));

    // Create event queue
    handle->event_queue = xQueueCreate(10, sizeof(fsm_event_t));
    if (!handle->event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        free(handle);
        return NULL;
    }

    // Initialize state
    handle->current_state = FSM_STATE_INIT;
    handle->previous_state = FSM_STATE_INIT;
    handle->is_running = false;
    handle->state_enter_time = get_time_ms();

    // Initialize statistics
    memset(&handle->statistics, 0, sizeof(fsm_statistics_t));

    // Initialize execution context
    memset(&handle->exec_context, 0, sizeof(fsm_execution_context_t));

    ESP_LOGI(TAG, "FSM Controller initialized in state: %s", state_names[handle->current_state]);

    return handle;
}

/**
 * @brief Deinitialize FSM controller
 */
void fsm_controller_deinit(fsm_controller_handle_t handle) {
    if (!handle) {
        return;
    }

    handle->is_running = false;

    if (handle->event_queue) {
        vQueueDelete(handle->event_queue);
    }

    free(handle);
    ESP_LOGI(TAG, "FSM Controller deinitialized");
}

/**
 * @brief Start FSM controller
 */
bool fsm_controller_start(fsm_controller_handle_t handle) {
    if (!handle) {
        return false;
    }

    handle->is_running = true;
    ESP_LOGI(TAG, "FSM Controller started");

    // Post INIT_DONE event to transition from INIT to IDLE state
    if (!fsm_controller_post_event(handle, FSM_EVENT_INIT_DONE)) {
        ESP_LOGW(TAG, "Failed to post INIT_DONE event");
        return false;
    }

    return true;
}

/**
 * @brief Stop FSM controller
 */
void fsm_controller_stop(fsm_controller_handle_t handle) {
    if (!handle) {
        return;
    }

    handle->is_running = false;
    ESP_LOGI(TAG, "FSM Controller stopped");
}

/**
 * @brief Find next state for given event
 */
static fsm_state_t find_next_state(fsm_state_t current_state, fsm_event_t event) {
    for (size_t i = 0; i < NUM_TRANSITIONS; i++) {
        if (state_transitions[i].from_state == current_state &&
            state_transitions[i].event == event) {
            return state_transitions[i].to_state;
        }
    }

    return FSM_STATE_COUNT; // Invalid state
}

/**
 * @brief Transition to new state
 */
static bool transition_to_state(fsm_controller_handle_t handle, fsm_state_t new_state) {
    if (!handle || new_state >= FSM_STATE_COUNT) {
        return false;
    }

    fsm_state_t old_state = handle->current_state;

    // Call exit callback for current state
    if (handle->callbacks[old_state].on_exit) {
        if (!handle->callbacks[old_state].on_exit(handle->callbacks[old_state].exit_user_data)) {
            ESP_LOGW(TAG, "Exit callback failed for state %s", state_names[old_state]);
        }
    }

    // Update statistics
    if (handle->config.enable_statistics) {
        uint32_t time_in_state = get_time_ms() - handle->state_enter_time;
        handle->statistics.state_duration_ms[old_state] += time_in_state;
        handle->statistics.state_enter_count[new_state]++;

        // Track errors
        if (new_state == FSM_STATE_CALIBRATION_ERROR ||
            new_state == FSM_STATE_HEATING_ERROR ||
            new_state == FSM_STATE_DATA_ERROR ||
            new_state == FSM_STATE_LOCK) {
            handle->statistics.error_count++;
        }

        // Track completed tasks
        if (new_state == FSM_STATE_IDLE && old_state == FSM_STATE_NORMAL_EXIT) {
            handle->statistics.task_completed_count++;
        }
    }

    // Update state
    handle->previous_state = old_state;
    handle->current_state = new_state;
    handle->state_enter_time = get_time_ms();

    // Reset execution context for new state
    memset(&handle->exec_context, 0, sizeof(fsm_execution_context_t));
    handle->exec_context.start_time_ms = get_time_ms();

    if (handle->config.enable_logging) {
        ESP_LOGI(TAG, "State transition: %s -> %s",
                 state_names[old_state], state_names[new_state]);
    }

    // Call enter callback for new state
    if (handle->callbacks[new_state].on_enter) {
        if (!handle->callbacks[new_state].on_enter(handle->callbacks[new_state].enter_user_data)) {
            ESP_LOGW(TAG, "Enter callback failed for state %s", state_names[new_state]);
        }
    }

    return true;
}

/**
 * @brief Process FSM
 */
void fsm_controller_process(fsm_controller_handle_t handle) {
    if (!handle || !handle->is_running) {
        return;
    }

    // Process pending events
    fsm_event_t event;
    if (xQueueReceive(handle->event_queue, &event, 0) == pdTRUE) {
        fsm_state_t next_state = find_next_state(handle->current_state, event);

        if (next_state < FSM_STATE_COUNT) {
            if (handle->config.enable_logging) {
                ESP_LOGI(TAG, "Processing event: %s in state: %s",
                         event_names[event], state_names[handle->current_state]);
            }
            transition_to_state(handle, next_state);
        } else {
            ESP_LOGW(TAG, "Invalid transition: event %s not valid in state %s",
                     event_names[event], state_names[handle->current_state]);
        }
    }

    // Execute current state callback
    if (handle->callbacks[handle->current_state].on_execute) {
        if (!handle->callbacks[handle->current_state].on_execute(
                handle->callbacks[handle->current_state].execute_user_data)) {
            // Execute callback returned false - could indicate an error
            ESP_LOGD(TAG, "Execute callback returned false for state %s",
                     state_names[handle->current_state]);
        }
    }
}

/**
 * @brief Post an event
 */
bool fsm_controller_post_event(fsm_controller_handle_t handle, fsm_event_t event) {
    if (!handle || event >= FSM_EVENT_COUNT) {
        return false;
    }

    if (xQueueSend(handle->event_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event: %s", event_names[event]);
        return false;
    }

    return true;
}

/**
 * @brief Get current state
 */
fsm_state_t fsm_controller_get_state(fsm_controller_handle_t handle) {
    if (!handle) {
        return FSM_STATE_COUNT;
    }

    return handle->current_state;
}

/**
 * @brief Get state color category
 */
fsm_state_color_t fsm_controller_get_state_color(fsm_state_t state) {
    switch (state) {
        case FSM_STATE_IDLE:
        case FSM_STATE_READY:
        case FSM_STATE_PAUSED:
        case FSM_STATE_LOCK:
            return FSM_COLOR_YELLOW;

        case FSM_STATE_CALIBRATION:
        case FSM_STATE_HEATING:
        case FSM_STATE_EXECUTING:
        case FSM_STATE_NORMAL_EXIT:
            return FSM_COLOR_YELLOW;

        case FSM_STATE_CALIBRATION_ERROR:
        case FSM_STATE_HEATING_ERROR:
        case FSM_STATE_DATA_ERROR:
            return FSM_COLOR_RED;

        case FSM_STATE_INIT:
            return FSM_COLOR_ASH;

        case FSM_STATE_MANUAL_CONTROL:
            return FSM_COLOR_OTHER;

        default:
            return FSM_COLOR_OTHER;
    }
}

/**
 * @brief Get state name
 */
const char* fsm_controller_get_state_name(fsm_state_t state) {
    if (state < FSM_STATE_COUNT) {
        return state_names[state];
    }

    return "UNKNOWN";
}

/**
 * @brief Get event name
 */
const char* fsm_controller_get_event_name(fsm_event_t event) {
    if (event < FSM_EVENT_COUNT) {
        return event_names[event];
    }

    return "UNKNOWN";
}

/**
 * @brief Register enter callback
 */
bool fsm_controller_register_enter_callback(fsm_controller_handle_t handle,
                                            fsm_state_t state,
                                            fsm_state_enter_callback_t callback,
                                            void* user_data) {
    if (!handle || state >= FSM_STATE_COUNT) {
        return false;
    }

    handle->callbacks[state].on_enter = callback;
    handle->callbacks[state].enter_user_data = user_data;
    return true;
}

/**
 * @brief Register exit callback
 */
bool fsm_controller_register_exit_callback(fsm_controller_handle_t handle,
                                           fsm_state_t state,
                                           fsm_state_exit_callback_t callback,
                                           void* user_data) {
    if (!handle || state >= FSM_STATE_COUNT) {
        return false;
    }

    handle->callbacks[state].on_exit = callback;
    handle->callbacks[state].exit_user_data = user_data;
    return true;
}

/**
 * @brief Register execute callback
 */
bool fsm_controller_register_execute_callback(fsm_controller_handle_t handle,
                                              fsm_state_t state,
                                              fsm_state_execute_callback_t callback,
                                              void* user_data) {
    if (!handle || state >= FSM_STATE_COUNT) {
        return false;
    }

    handle->callbacks[state].on_execute = callback;
    handle->callbacks[state].execute_user_data = user_data;
    return true;
}

/**
 * @brief Get statistics
 */
bool fsm_controller_get_statistics(fsm_controller_handle_t handle, fsm_statistics_t* stats) {
    if (!handle || !stats) {
        return false;
    }

    memcpy(stats, &handle->statistics, sizeof(fsm_statistics_t));
    return true;
}

/**
 * @brief Reset statistics
 */
void fsm_controller_reset_statistics(fsm_controller_handle_t handle) {
    if (!handle) return;

    memset(&handle->statistics, 0, sizeof(fsm_statistics_t));
    ESP_LOGI(TAG, "Statistics reset");
}

/**
 * @brief Check if in error state
 */
bool fsm_controller_is_in_error(fsm_controller_handle_t handle) {
    if (!handle) {
        return false;
    }

    return (handle->current_state == FSM_STATE_CALIBRATION_ERROR ||
            handle->current_state == FSM_STATE_HEATING_ERROR ||
            handle->current_state == FSM_STATE_DATA_ERROR ||
            handle->current_state == FSM_STATE_LOCK);
}

/**
 * @brief Get time in current state
 */
uint32_t fsm_controller_get_time_in_state(fsm_controller_handle_t handle) {
    if (!handle) {
        return 0;
    }

    return get_time_ms() - handle->state_enter_time;
}

/**
 * @brief Get execution context
 */
fsm_execution_context_t* fsm_controller_get_execution_context(fsm_controller_handle_t handle) {
    if (!handle) {
        return NULL;
    }

    return &handle->exec_context;
}

/**
 * @brief Initialize execution context
 */
void fsm_execution_context_init(fsm_execution_context_t* context) {
    if (!context) {
        return;
    }

    memset(context, 0, sizeof(fsm_execution_context_t));
    context->start_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Get FSM configuration
 */
const fsm_config_t* fsm_controller_get_config(fsm_controller_handle_t handle) {
    if (!handle) {
        return NULL;
    }

    return &handle->config;
}
