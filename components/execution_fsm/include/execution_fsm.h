/**
 * @file execution_fsm.h
 * @brief Execution sub-FSM for soldering operations
 *
 * Handles physical movement sequence:
 * - Move XY to solder point at safe Z height
 * - Lower Z to soldering height
 * - Feed solder wire for specified duration
 * - Raise Z to safe height
 * - Return all axes to home position
 *
 * Note: Post-execution cleanup (cooldown, safety checks) handled by parent FSM
 */

#ifndef EXECUTION_FSM_H
#define EXECUTION_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Execution Sub-FSM States:
 * IDLE         - Initial state
 * MOVE_TO_POINT - Move XY at safe Z height
 * MOVE_DOWN    - Lower Z to soldering height
 * SOLDERING    - Feed solder wire
 * MOVE_UP      - Raise Z to safe height
 * RETURN_HOME  - Return all axes to home
 * COMPLETE     - All operations finished
 */
typedef enum {
    EXEC_STATE_IDLE = 0,        // Waiting to start
    EXEC_STATE_MOVE_TO_POINT,   // Move XY to next point (Z safe)
    EXEC_STATE_MOVE_DOWN,       // Lower Z to solder position
    EXEC_STATE_SOLDERING,       // Feed solder wire
    EXEC_STATE_MOVE_UP,         // Raise Z to safe height
    EXEC_STATE_RETURN_HOME,     // Return to origin
    EXEC_STATE_COMPLETE,        // All done
    EXEC_STATE_COUNT
} exec_sub_state_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    bool solder;
    uint32_t solder_time_ms;
} solder_point_t;

typedef struct {
    int32_t safe_z_height;          // Z height for XY movements (steps)
    int32_t soldering_z_height;     // Z height for soldering (steps)
    int32_t home_x;                 // Home X coordinate (steps)
    int32_t home_y;                 // Home Y coordinate (steps)
    int32_t home_z;                 // Home Z coordinate (steps)
} execution_config_t;

typedef struct {
    exec_sub_state_t sub_state;
    int current_point_index;
    uint32_t state_enter_time;
    uint32_t solder_start_time;
    int32_t solder_start_pos;
    int solder_points_completed;
    bool operation_in_progress;
    execution_config_t config;      // Configuration parameters
    void* gcode_parser_handle;      // GCode parser handle (opaque)
    bool use_gcode;                 // True if executing from GCode, false for point array
} execution_sub_fsm_t;

void exec_sub_fsm_init(execution_sub_fsm_t* fsm, const execution_config_t* config);
void exec_sub_fsm_process(execution_sub_fsm_t* fsm, const solder_point_t* points, int num_points);
exec_sub_state_t exec_sub_fsm_get_state(const execution_sub_fsm_t* fsm);
int exec_sub_fsm_get_completed_count(const execution_sub_fsm_t* fsm);
const char* exec_sub_fsm_get_state_name(exec_sub_state_t state);
execution_config_t exec_sub_fsm_get_default_config(void);

// GCode execution functions
bool exec_sub_fsm_load_gcode_from_ram(execution_sub_fsm_t* fsm, const char* gcode_buffer, size_t buffer_size);
void exec_sub_fsm_process_gcode(execution_sub_fsm_t* fsm);
void exec_sub_fsm_cleanup_gcode(execution_sub_fsm_t* fsm);

#ifdef __cplusplus
}
#endif

#endif // EXECUTION_FSM_H
