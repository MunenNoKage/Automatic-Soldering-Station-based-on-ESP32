/**
 * @file fsm_controller.h
 * @brief Finite State Machine controller for automatic soldering station
 *
 * This component implements the core FSM logic that coordinates all system operations.
 * It manages state transitions, error handling, and system workflow.
 *
 * @author UCU Automatic Soldering Station Team
 * @date 2025
 */

#ifndef FSM_CONTROLLER_H
#define FSM_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FSM System States
 */
typedef enum {
    FSM_STATE_INIT = 0,              // Initial power-on state (Ash)
    FSM_STATE_IDLE,                  // Idle/standby state (Yellow)
    FSM_STATE_MANUAL_CONTROL,        // Manual control mode (Other)
    FSM_STATE_CALIBRATION,           // Calibration process (Green)
    FSM_STATE_READY,                 // Ready to execute (Yellow)
    FSM_STATE_HEATING,               // Heating soldering iron (Green)
    FSM_STATE_EXECUTING,             // Executing soldering task (Green)
    FSM_STATE_PAUSED,                // Task paused (Yellow)
    FSM_STATE_NORMAL_EXIT,           // Task cleanup: cooldown, safety checks (Yellow)
    FSM_STATE_CALIBRATION_ERROR,     // Calibration error (Red)
    FSM_STATE_HEATING_ERROR,         // Heating/temperature error (Red)
    FSM_STATE_DATA_ERROR,            // Sensor data error (Red)
    FSM_STATE_LOCK,                  // System locked due to error (Red)
    FSM_STATE_COUNT                  // Total number of states
} fsm_state_t;

/**
 * @brief FSM Events (triggers for state transitions)
 */
typedef enum {
    FSM_EVENT_INIT_DONE = 0,         // Initialization completed
    FSM_EVENT_SELECT_MANUAL,         // User selected manual control
    FSM_EVENT_EXIT_MANUAL,           // Exit manual control
    FSM_EVENT_TASK_SENT,             // New task was sent
    FSM_EVENT_REQUEST_CALIBRATION,   // Calibration requested
    FSM_EVENT_CALIBRATION_SUCCESS,   // Calibration successful
    FSM_EVENT_CALIBRATION_ERROR,     // Calibration error (no contact)
    FSM_EVENT_CANCEL_TASK,           // Task cancelled by user
    FSM_EVENT_CALIBRATION_DONE,      // Calibration request completed
    FSM_EVENT_TASK_APPROVED,         // Task approved to start
    FSM_EVENT_HEATING_SUCCESS,       // Target temperature reached
    FSM_EVENT_HEATING_ERROR,         // Heating error occurred
    FSM_EVENT_PAUSE_REQUEST,         // Pause requested (IR sensor)
    FSM_EVENT_TASK_DONE,             // Task completed successfully
    FSM_EVENT_DATA_ERROR,            // Bad data from sensors
    FSM_EVENT_EXIT_REQUEST,          // Exit requested from paused state
    FSM_EVENT_CONTINUE_TASK,         // Continue task from pause
    FSM_EVENT_COOLDOWN_COMPLETE,     // Iron cooldown completed successfully
    FSM_EVENT_COOLING_ERROR,         // Cooling error occurred
    FSM_EVENT_COUNT                  // Total number of events
} fsm_event_t;

/**
 * @brief State color categorization for visualization
 */
typedef enum {
    FSM_COLOR_YELLOW,    // Standby states
    FSM_COLOR_GREEN,     // Process states
    FSM_COLOR_RED,       // Error states
    FSM_COLOR_ASH,       // Init state
    FSM_COLOR_OTHER      // Other states
} fsm_state_color_t;

/**
 * @brief FSM statistics and monitoring
 */
typedef struct {
    uint32_t state_enter_count[FSM_STATE_COUNT];    // Number of times each state was entered
    uint32_t state_duration_ms[FSM_STATE_COUNT];    // Total time spent in each state (ms)
    uint32_t last_state_enter_time;                 // Timestamp of last state entry
    uint32_t error_count;                            // Total number of errors
    uint32_t task_completed_count;                   // Number of successfully completed tasks
} fsm_statistics_t;

/**
 * @brief FSM Controller configuration
 */
typedef struct {
    uint32_t tick_rate_ms;              // FSM update rate in milliseconds
    bool enable_logging;                 // Enable state transition logging
    bool enable_statistics;              // Enable statistics collection
    float target_temperature;            // Target temperature for heating (°C)
    float temperature_tolerance;         // Temperature tolerance (±°C)
    uint32_t heating_timeout_ms;         // Maximum heating time before error
    uint32_t calibration_timeout_ms;     // Maximum calibration time
    float safe_temperature;              // Safe temperature for cooldown (°C)
    uint32_t cooldown_timeout_ms;        // Maximum cooldown time before error
} fsm_config_t;

/**
 * @brief Execution context structure
 *
 * This structure can be used to maintain state across FSM ticks during
 * long-running operations. User application can extend this or use their own.
 */
typedef struct {
    uint32_t start_time_ms;             // Operation start time
    uint32_t iteration_count;            // Number of iterations/steps completed
    bool operation_complete;             // Flag indicating completion
    void* user_context;                  // User-defined context data
} fsm_execution_context_t;

/**
 * @brief FSM Controller handle
 */
typedef struct fsm_controller_s* fsm_controller_handle_t;

/**
 * @brief Callback function types for FSM actions
 *
 * These callbacks are called when entering/exiting states or during state execution.
 * Return false to indicate an error occurred.
 */
typedef bool (*fsm_state_enter_callback_t)(void* user_data);
typedef bool (*fsm_state_exit_callback_t)(void* user_data);
typedef bool (*fsm_state_execute_callback_t)(void* user_data);

/**
 * @brief Initialize FSM controller
 *
 * @param config Configuration structure
 * @return FSM controller handle, or NULL on failure
 */
fsm_controller_handle_t fsm_controller_init(const fsm_config_t* config);

/**
 * @brief Deinitialize FSM controller
 *
 * @param handle FSM controller handle
 */
void fsm_controller_deinit(fsm_controller_handle_t handle);

/**
 * @brief Start FSM controller (begins processing)
 *
 * @param handle FSM controller handle
 * @return true on success, false on failure
 */
bool fsm_controller_start(fsm_controller_handle_t handle);

/**
 * @brief Stop FSM controller
 *
 * @param handle FSM controller handle
 */
void fsm_controller_stop(fsm_controller_handle_t handle);

/**
 * @brief Process FSM
 *
 * This function processes the current state and handles transitions.
 * Should be called periodically based on tick_rate_ms.
 *
 * @param handle FSM controller handle
 */
void fsm_controller_process(fsm_controller_handle_t handle);

/**
 * @brief Post an event to trigger state transition
 *
 * @param handle FSM controller handle
 * @param event Event to post
 * @return true if event was accepted, false otherwise
 */
bool fsm_controller_post_event(fsm_controller_handle_t handle, fsm_event_t event);

/**
 * @brief Get current FSM state
 *
 * @param handle FSM controller handle
 * @return Current state
 */
fsm_state_t fsm_controller_get_state(fsm_controller_handle_t handle);

/**
 * @brief Get state color category
 *
 * @param state State to query
 * @return Color category
 */
fsm_state_color_t fsm_controller_get_state_color(fsm_state_t state);

/**
 * @brief Get state name as string
 *
 * @param state State to query
 * @return State name string
 */
const char* fsm_controller_get_state_name(fsm_state_t state);

/**
 * @brief Get event name as string
 *
 * @param event Event to query
 * @return Event name string
 */
const char* fsm_controller_get_event_name(fsm_event_t event);

/**
 * @brief Register callback for state entry
 *
 * @param handle FSM controller handle
 * @param state State to register callback for
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return true on success, false on failure
 */
bool fsm_controller_register_enter_callback(fsm_controller_handle_t handle,
                                            fsm_state_t state,
                                            fsm_state_enter_callback_t callback,
                                            void* user_data);

/**
 * @brief Register callback for state exit
 *
 * @param handle FSM controller handle
 * @param state State to register callback for
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return true on success, false on failure
 */
bool fsm_controller_register_exit_callback(fsm_controller_handle_t handle,
                                           fsm_state_t state,
                                           fsm_state_exit_callback_t callback,
                                           void* user_data);

/**
 * @brief Register callback for state execution
 *
 * This callback is called repeatedly while in the state (every process cycle).
 *
 * @param handle FSM controller handle
 * @param state State to register callback for
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return true on success, false on failure
 */
bool fsm_controller_register_execute_callback(fsm_controller_handle_t handle,
                                              fsm_state_t state,
                                              fsm_state_execute_callback_t callback,
                                              void* user_data);

/**
 * @brief Get FSM statistics
 *
 * @param handle FSM controller handle
 * @param stats Pointer to statistics structure to fill
 * @return true on success, false on failure
 */
bool fsm_controller_get_statistics(fsm_controller_handle_t handle, fsm_statistics_t* stats);

/**
 * @brief Reset FSM statistics
 *
 * @param handle FSM controller handle
 */
void fsm_controller_reset_statistics(fsm_controller_handle_t handle);

/**
 * @brief Check if FSM is in an error state
 *
 * @param handle FSM controller handle
 * @return true if in error state, false otherwise
 */
bool fsm_controller_is_in_error(fsm_controller_handle_t handle);

/**
 * @brief Get time in current state (milliseconds)
 *
 * @param handle FSM controller handle
 * @return Time in current state in milliseconds
 */
uint32_t fsm_controller_get_time_in_state(fsm_controller_handle_t handle);

/**
 * @brief Get pointer to execution context for current state
 *
 * This allows callbacks to maintain state across FSM ticks for
 * non-blocking operations. The context is reset on state transitions.
 *
 * @param handle FSM controller handle
 * @return Pointer to execution context, or NULL on error
 */
fsm_execution_context_t* fsm_controller_get_execution_context(fsm_controller_handle_t handle);

/**
 * @brief Initialize execution context helper
 *
 * Convenience function to initialize context with start time
 *
 * @param context Pointer to context to initialize
 */
void fsm_execution_context_init(fsm_execution_context_t* context);

/**
 * @brief Get FSM configuration
 *
 * @param handle FSM controller handle
 * @return Pointer to configuration structure, or NULL on error
 */
const fsm_config_t* fsm_controller_get_config(fsm_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // FSM_CONTROLLER_H
