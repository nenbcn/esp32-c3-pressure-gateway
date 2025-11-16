#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "signal_parameters.h"

// =============================================================================
// GLOBAL DATA STRUCTURES AND QUEUES
// =============================================================================
// This file centralizes all shared data structures, enums, and queue definitions
// used across the pressure monitoring system. All global colas and mutexes
// are declared here to ensure consistency and avoid circular dependencies.

// =============================================================================
// PRESSURE PROCESSING ENUMS
// =============================================================================

// Event types for pressure events
typedef enum {
    EVENT_TYPE_STABLE,                 // Stable pressure interval
    EVENT_TYPE_RISING,                 // Rising pressure event
    EVENT_TYPE_FALLING,                // Falling pressure event
    EVENT_TYPE_OSCILLATION             // Oscillating pressure event
} EventType;

// Trigger reasons for event detection
typedef enum {
    TRIGGER_DERIVATIVE_RISING,         // Positive derivative exceeded threshold
    TRIGGER_DERIVATIVE_FALLING,        // Negative derivative exceeded threshold
    TRIGGER_TIMEOUT,                   // Interval closed due to timeout
    TRIGGER_BUFFER_FULL                // Interval closed due to buffer capacity
} TriggerReason;

// Signal state machine states
typedef enum {
    SIGNAL_STATE_STABLE,               // Derivative below threshold (stable period)
    SIGNAL_STATE_CHANGING              // Derivative above threshold (changing period)
} SignalState;

// =============================================================================
// PRESSURE DATA STRUCTURES
// =============================================================================

// Raw pressure reading (Cola 1: pressure_reader → pressure_telemetry)
typedef struct {
    uint64_t timestamp;                // Sample timestamp (millis)
    uint32_t rawValue;                 // RAW 24-bit sensor value
    bool isValid;                      // Validation result (limits + variation)
} PressureReading;

// Processed sample with filtering and derivative info
typedef struct {
    uint64_t timestamp;                // Sample timestamp
    uint32_t filteredValue;            // EPA2-filtered pressure value
    float derivative;                  // Calculated derivative
} PressureSample;

// Complete pressure event (Cola 2: pressure_telemetry → message_formatter)
typedef struct {
    uint64_t startTimestamp;           // Event start time
    uint64_t endTimestamp;             // Event end time
    EventType type;                    // Event classification
    uint32_t startValue;               // Starting pressure (EPA2 filtered)
    uint32_t endValue;                 // Ending pressure (EPA2 filtered)
    uint16_t sampleCount;              // Total samples in event
    TriggerReason triggerReason;       // Detection trigger cause
    bool hasDetailedSamples;           // True if samples array contains data
    PressureSample samples[MAX_SAMPLES_PER_EVENT];  // Detailed sample array
} PressureEvent;

// =============================================================================
// MQTT MESSAGE STRUCTURE
// =============================================================================

// Generic MQTT message (Cola 3: message_formatter → mqtt_handler)
typedef struct {
    char topic[128];                   // Complete MQTT topic
    char payload[4096];                // JSON-serialized payload
    uint8_t qos;                       // QoS level (0 or 1)
    bool retain;                       // Retain flag
} MqttMessage;

// =============================================================================
// SIGNAL PROCESSING STATE
// =============================================================================

// Derivative calculation window (circular buffer)
typedef struct {
    float values[DERIVATIVE_WINDOW_SIZE];          // EPA2 values for calculation
    uint64_t timestamps[DERIVATIVE_WINDOW_SIZE];   // Corresponding timestamps
    uint16_t writeIndex;               // Current write position
    uint16_t count;                    // Valid samples in buffer
    float lastDerivative;              // Last calculated derivative
} DerivativeWindow;

// Signal state machine context
typedef struct {
    SignalState currentState;          // Current processing state
    uint64_t stateStartTime;          // When current state began
    uint64_t lastEventTime;           // Last event detection time
    uint32_t eventsDetected;          // Total events counter
    bool transitionPending;           // State transition in progress
} SignalStateMachine;

// Statistics accumulator for stable periods
typedef struct {
    uint32_t minValue;                // Minimum value in period
    uint32_t maxValue;                // Maximum value in period
    uint64_t sumValues;               // Sum for average calculation
    uint32_t sampleCount;             // Sample count for statistics
    uint64_t periodStartTime;         // Period start timestamp
} StableAccumulator;

// =============================================================================
// GLOBAL QUEUE DECLARATIONS
// =============================================================================
// All queues are declared here and created in their respective modules.
// This centralizes queue management and prevents duplicate declarations.

// Queue 1: RAW pressure samples (100Hz from sensor)
extern QueueHandle_t g_pressureQueue;

// Queue 2: Detected pressure events (processed intervals)
extern QueueHandle_t g_pressureEventQueue;

// Queue 3: Formatted MQTT messages (JSON ready for transmission)
extern QueueHandle_t g_mqttQueue;

// =============================================================================
// GLOBAL MUTEX DECLARATIONS
// =============================================================================

extern SemaphoreHandle_t g_i2cMutex;           // I2C bus access protection
extern SemaphoreHandle_t g_stateMutex;         // System state protection
extern SemaphoreHandle_t g_wifiMutex;          // WiFi operations protection
extern SemaphoreHandle_t g_mqttMutex;          // MQTT client protection
extern SemaphoreHandle_t g_eepromMutex;        // EEPROM access protection

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Gets string representation of event type.
 * @param type The event type enum
 * @return String description for JSON serialization
 */
const char* getEventTypeString(EventType type);

/**
 * @brief Gets string representation of trigger reason.
 * @param reason The trigger reason enum
 * @return String description for JSON serialization
 */
const char* getTriggerReasonString(TriggerReason reason);

/**
 * @brief Validates that a PressureReading structure is properly formed.
 * @param reading Pointer to the reading to validate
 * @return true if structure is valid, false otherwise
 */
bool validatePressureReading(const PressureReading* reading);

/**
 * @brief Validates that a PressureEvent structure is properly formed.
 * @param event Pointer to the event to validate
 * @return true if structure is valid, false otherwise
 */
bool validatePressureEvent(const PressureEvent* event);

#endif // DATA_TYPES_H