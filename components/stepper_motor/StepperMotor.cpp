/**
 * @file StepperMotor.cpp
 * @brief Implementation of C++ stepper motor wrapper
 *
 * Provides object-oriented interface to stepper motor HAL.
 */

#include "StepperMotor.hpp"
#include "esp_log.h"
#include <stdexcept>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // Add this line for vTaskDelay

static const char *TAG = "StepperMotor";

// Constructor
StepperMotor::StepperMotor(const stepper_motor_config_t& config, uint32_t steps_per_mm, stepper_direction_t positive_direction) :
    position_(0),
    handle_(stepper_motor_hal_init(&config)),
    target_position_(0),
    steps_per_mm(steps_per_mm),
    positive_direction_(positive_direction)
{
    if (handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize stepper motor");
        // Cannot throw in ESP-ISTEPPER_DIR_CLOCKWISEDF (exceptions disabled)
        // User should check isInitialized() after construction
    } else {
        ESP_LOGI(TAG, "StepperMotor initialized with %lu steps/mm", steps_per_mm);
    }
}

// Destructor
StepperMotor::~StepperMotor() {
    if (handle_ != nullptr) {
        stepper_motor_hal_deinit(handle_);
        handle_ = nullptr;
    }
}

// Move constructor
StepperMotor::StepperMotor(StepperMotor&& other) noexcept :
    position_(other.position_),
    handle_(other.handle_),
    target_position_(other.target_position_),
    steps_per_mm(other.steps_per_mm)
{
    other.handle_ = nullptr;
    other.position_ = 0;
    other.target_position_ = 0;
}

// Move assignment operator
StepperMotor& StepperMotor::operator=(StepperMotor&& other) noexcept {
    if (this != &other) {
        // Clean up existing handle
        if (handle_ != nullptr) {
            stepper_motor_hal_deinit(handle_);
        }

        // Move data
        position_ = other.position_;
        handle_ = other.handle_;
        target_position_ = other.target_position_;
        steps_per_mm = other.steps_per_mm;

        // Reset other
        other.handle_ = nullptr;
        other.position_ = 0;
        other.target_position_ = 0;
    }
    return *this;
}

void StepperMotor::setEnable(bool enable) {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }
    stepper_motor_hal_set_enable(handle_, enable);
}

void StepperMotor::setDirection(stepper_direction_t direction) {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }
    stepper_motor_hal_set_direction(handle_, direction);
}

void StepperMotor::step() {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }
    stepper_motor_hal_step(handle_, 200);

    // Update position based on direction
    if (stepper_motor_hal_get_direction(handle_) == STEPPER_DIR_CLOCKWISE) {
        position_++;
    } else {
        position_--;
    }
}

void StepperMotor::stepMultiple(uint32_t steps) {
    if (!handle_) return;

    // Update position based on direction
    if (stepper_motor_hal_get_direction(handle_) == positive_direction_) {
        position_ += steps;
    } else {
        position_ -= steps;
    }

    stepper_motor_hal_step_multiple(handle_, steps);

    if (isEndpointReached()) {
        ESP_LOGW(TAG, "Endpoint reached during stepMultiple. Reseting position to 0");
        position_ = 0;
    }
}

void StepperMotor::setTargetPosition(int32_t position) {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }
    ESP_LOGI(TAG, "Setting target position to %d", position);
    target_position_ = std::max(int32_t(0), position);
    ESP_LOGI(TAG, "Target position set to %d", target_position_);
}

void StepperMotor::stepToTarget() {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }

    int32_t current_pos = getPosition();

    if (current_pos < target_position_) {
        // Move forward
        stepper_motor_hal_set_direction(handle_, STEPPER_DIR_CLOCKWISE);
        step();  // Use step() method which handles position tracking
    } else if (current_pos > target_position_) {
        // Move backward
        stepper_motor_hal_set_direction(handle_, STEPPER_DIR_COUNTERCLOCKWISE);
        step();  // Use step() method which handles position tracking
    }
    // If current_pos == target_position_, do nothing
}

void StepperMotor::stepMultipleToTarget(uint32_t max_steps) {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }

    int32_t current_pos = getPosition();
    int32_t remaining_steps = target_position_ - current_pos;

    if (remaining_steps == 0) {
        return; // Already at target
    }

    // Determine direction based on remaining_steps
    stepper_direction_t dir;
    if (remaining_steps > 0) {
        dir = positive_direction_;
    } else {
        dir = (stepper_direction_t)((positive_direction_ == STEPPER_DIR_CLOCKWISE) ? STEPPER_DIR_COUNTERCLOCKWISE : STEPPER_DIR_CLOCKWISE);
    }

    // Set direction
    stepper_motor_hal_set_direction(handle_, dir);

    // Limit steps to max_steps
    uint32_t steps_to_execute = std::min(static_cast<uint32_t>(remaining_steps), max_steps);

    ESP_LOGI(TAG, "Current position: %d, Target: %d", current_pos, target_position_);
    ESP_LOGI(TAG, "Remaining steps: %d, Steps to execute: %u", remaining_steps, steps_to_execute);

    stepMultiple(steps_to_execute);
}

int32_t StepperMotor::getPosition() const {
    return position_;
}

int32_t StepperMotor::getTargetPosition() const {
    return target_position_;
}

void StepperMotor::resetPosition() {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }
    position_ = 0;
    target_position_ = 0;
    ESP_LOGI(TAG, "Position reset to 0");
}

bool StepperMotor::isEndpointReached() const {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return false;
    }
    return stepper_motor_hal_endpoint_reached(handle_);
}

void StepperMotor::calibrate() {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "Motor not initialized");
        return;
    }

    ESP_LOGI(TAG, "Calibrating motor...");

    // Move towards endpoint until switch is triggered
    stepper_motor_hal_set_direction(handle_, STEPPER_DIR_COUNTERCLOCKWISE);
    bool ennable_state = stepper_motor_hal_get_direction(handle_);
    setEnable(true);

    setDirection((positive_direction_ == STEPPER_DIR_CLOCKWISE) ? STEPPER_DIR_COUNTERCLOCKWISE : STEPPER_DIR_CLOCKWISE);

    while (!isEndpointReached()) {
        stepper_motor_hal_step_multiple(handle_, 100); // Step in small increments
        vTaskDelay(pdMS_TO_TICKS(2)); // Small delay to avoid busy-waiting
    }

    // Reset position to zero at endpoint
    resetPosition();

    setEnable(ennable_state);

    ESP_LOGI(TAG, "Motor calibrated to endpoint");
}

int32_t StepperMotor::microsteps_to_mm(int32_t microsteps) {
    return (int32_t)(microsteps / steps_per_mm);
}

int32_t StepperMotor::mm_to_microsteps(int32_t mm) {
    return (int32_t)(mm * steps_per_mm);
}