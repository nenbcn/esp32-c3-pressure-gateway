/*
 * data_types.cpp
 *
 * Implementation of utility functions for global data structures.
 * Contains string conversion functions and validation helpers for
 * pressure monitoring data types.
 */

#include "data_types.h"
#include "Log.h"

/**
 * @brief Gets string representation of event type.
 */
const char* getEventTypeString(EventType type) {
    switch (type) {
        case EVENT_TYPE_STABLE:      return "stable";
        case EVENT_TYPE_RISING:      return "rising";
        case EVENT_TYPE_FALLING:     return "falling";
        case EVENT_TYPE_OSCILLATION: return "oscillation";
        default:                     return "unknown";
    }
}

/**
 * @brief Gets string representation of trigger reason.
 */
const char* getTriggerReasonString(TriggerReason reason) {
    switch (reason) {
        case TRIGGER_DERIVATIVE_RISING:  return "derivative_rising";
        case TRIGGER_DERIVATIVE_FALLING: return "derivative_falling";
        case TRIGGER_TIMEOUT:            return "timeout";
        case TRIGGER_BUFFER_FULL:        return "buffer_full";
        default:                         return "unknown";
    }
}

/**
 * @brief Validates that a PressureReading structure is properly formed.
 */
bool validatePressureReading(const PressureReading* reading) {
    if (reading == nullptr) {
        return false;
    }
    
    // Validate timestamp (should be reasonable)
    if (reading->timestamp == 0) {
        return false;
    }
    
    // If marked as valid, raw value should be in acceptable range
    if (reading->isValid) {
        if (reading->rawValue <= RAW_VALUE_MIN || reading->rawValue >= RAW_VALUE_MAX) {
            Log::warn("[DataTypes] Invalid reading marked as valid: %lu", reading->rawValue);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Validates that a PressureEvent structure is properly formed.
 */
bool validatePressureEvent(const PressureEvent* event) {
    if (event == nullptr) {
        return false;
    }
    
    // Validate timestamps
    if (event->startTimestamp == 0 || event->endTimestamp == 0) {
        return false;
    }
    
    if (event->endTimestamp < event->startTimestamp) {
        Log::warn("[DataTypes] Event end timestamp before start: %llu < %llu", 
                  event->endTimestamp, event->startTimestamp);
        return false;
    }
    
    // Validate sample count
    if (event->sampleCount == 0) {
        Log::warn("[DataTypes] Event with zero samples");
        return false;
    }
    
    if (event->sampleCount > MAX_SAMPLES_PER_EVENT) {
        Log::warn("[DataTypes] Event sample count exceeds maximum: %u > %u", 
                  event->sampleCount, MAX_SAMPLES_PER_EVENT);
        return false;
    }
    
    // If detailed samples are claimed, validate first few samples
    if (event->hasDetailedSamples && event->sampleCount > 0) {
        // Check first sample has valid timestamp
        if (event->samples[0].timestamp == 0) {
            Log::warn("[DataTypes] Event detailed samples have invalid timestamps");
            return false;
        }
        
        // Check samples are within event timeframe
        if (event->samples[0].timestamp < event->startTimestamp) {
            Log::warn("[DataTypes] Event sample timestamp before event start");
            return false;
        }
    }
    
    // Validate event type
    if (event->type > EVENT_TYPE_OSCILLATION) {
        Log::warn("[DataTypes] Invalid event type: %d", (int)event->type);
        return false;
    }
    
    // Validate trigger reason
    if (event->triggerReason > TRIGGER_BUFFER_FULL) {
        Log::warn("[DataTypes] Invalid trigger reason: %d", (int)event->triggerReason);
        return false;
    }
    
    return true;
}