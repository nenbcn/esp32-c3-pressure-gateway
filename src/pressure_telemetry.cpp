/*
 * pressure_telemetry.cpp
 *
 * Pressure signal processing and event detection module.
 * This module focuses exclusively on signal processing: EPA filtering,
 * derivative calculation, and event detection. It does NOT handle JSON
 * formatting or MQTT communication - that's handled by message_formatter.
 *
 * Key Features:
 * - Double EPA filtering (primary + secondary) for noise reduction
 * - Derivative-based change detection with hysteresis
 * - Adaptive event classification (stable, rising, falling, oscillation)
 * - Statistical compression for stable periods
 * - Detailed sample capture for changing periods
 * 
 * Input:  g_pressureQueue (PressureReading from pressure_reader)
 * Output: g_pressureEventQueue (PressureEvent to message_formatter)
 */

#include "pressure_telemetry.h"
#include "system_state.h"
#include "data_types.h"

// External queue references (created by their respective modules)
extern QueueHandle_t g_pressureQueue;         // From pressure_reader
QueueHandle_t g_pressureEventQueue = NULL;    // Created here

// EPA filter state
static float primaryFiltered = 0.0f;
static float secondaryFiltered = 0.0f;
static bool filtersInitialized = false;

// Derivative calculation state
static DerivativeWindow derivativeWindow = {0};
static float filteredDerivative = 0.0f;

// Signal state machine
static SignalStateMachine stateMachine;

// Period accumulators
static StableAccumulator stableAccumulator = {0};
static PressureEvent currentEvent = {0};
static bool eventInProgress = false;

// Statistics
static uint32_t totalSamplesProcessed = 0;
static uint32_t stablePeriodsDetected = 0;
static uint32_t changingEventsDetected = 0;
static uint32_t eventsQueueFull = 0;

/**
 * @brief Initializes the pressure telemetry system.
 */
bool initializePressureTelemetry() {
    // Create event queue
    g_pressureEventQueue = xQueueCreate(PRESSURE_EVENT_QUEUE_SIZE, sizeof(PressureEvent));
    if (g_pressureEventQueue == NULL) {
        Log::error("[Telemetry] Failed to create pressure event queue");
        return false;
    }

    // Initialize EPA filters
    filtersInitialized = false;
    primaryFiltered = 0.0f;
    secondaryFiltered = 0.0f;

    // Initialize derivative window
    memset(&derivativeWindow, 0, sizeof(DerivativeWindow));
    filteredDerivative = 0.0f;

    // Initialize state machine
    stateMachine.currentState = SIGNAL_STATE_STABLE;
    stateMachine.stateStartTime = millis();
    stateMachine.lastEventTime = 0;
    stateMachine.eventsDetected = 0;
    stateMachine.transitionPending = false;

    // Initialize accumulators
    memset(&stableAccumulator, 0, sizeof(StableAccumulator));
    memset(&currentEvent, 0, sizeof(PressureEvent));
    eventInProgress = false;

    // Reset statistics
    totalSamplesProcessed = 0;
    stablePeriodsDetected = 0;
    changingEventsDetected = 0;
    eventsQueueFull = 0;

    Log::info("[Telemetry] Initialized signal processing - EPA α1=%0.2f α2=%0.2f, derivative window=%d", 
              EPA_ALPHA_PRIMARY, EPA_ALPHA_SECONDARY, DERIVATIVE_WINDOW_SIZE);
    return true;
}

/**
 * @brief Applies EPA (Exponential Moving Average) filter.
 */
static inline float applyEPAFilter(float newValue, float prevFiltered, float alpha) {
    return alpha * newValue + (1.0f - alpha) * prevFiltered;
}

/**
 * @brief Calculates derivative from the sliding window buffer.
 */
float calculateDerivative(const DerivativeWindow* window) {
    if (window->count < 2) {
        return 0.0f;
    }

    // Use oldest and newest values for derivative calculation
    uint16_t oldestIndex = (window->writeIndex + DERIVATIVE_WINDOW_SIZE - window->count) % DERIVATIVE_WINDOW_SIZE;
    uint16_t newestIndex = (window->writeIndex + DERIVATIVE_WINDOW_SIZE - 1) % DERIVATIVE_WINDOW_SIZE;
    
    float valueDiff = window->values[newestIndex] - window->values[oldestIndex];
    uint64_t timeDiff = window->timestamps[newestIndex] - window->timestamps[oldestIndex];
    
    if (timeDiff == 0) {
        return 0.0f;
    }
    
    // Convert to units per second
    return (valueDiff * 1000.0f) / (float)timeDiff;
}

/**
 * @brief Adds a sample to the derivative window buffer.
 */
void addToDerivativeWindow(DerivativeWindow* window, float value, uint64_t timestamp) {
    window->values[window->writeIndex] = value;
    window->timestamps[window->writeIndex] = timestamp;
    
    window->writeIndex = (window->writeIndex + 1) % DERIVATIVE_WINDOW_SIZE;
    
    if (window->count < DERIVATIVE_WINDOW_SIZE) {
        window->count++;
    }
}

/**
 * @brief Determines event type based on derivative and pressure change.
 */
static EventType classifyEvent(float avgDerivative, uint32_t startValue, uint32_t endValue) {
    float pressureChange = (float)((int32_t)endValue - (int32_t)startValue);
    
    // Classify based on overall trend and derivative magnitude
    if (fabs(avgDerivative) < (DERIVATIVE_THRESHOLD * 0.3f)) {
        return EVENT_TYPE_STABLE;  // Very small changes
    } else if (pressureChange > 0 && avgDerivative > 0) {
        return EVENT_TYPE_RISING;   // Positive trend
    } else if (pressureChange < 0 && avgDerivative < 0) {
        return EVENT_TYPE_FALLING;  // Negative trend
    } else {
        return EVENT_TYPE_OSCILLATION;  // Mixed or contradictory signals
    }
}

/**
 * @brief Updates signal state machine based on current derivative value.
 */
bool updateSignalStateMachine(SignalStateMachine* stateMachine, float currentDerivative, uint64_t currentTime) {
    bool stateChanged = false;
    float absDerivative = fabs(currentDerivative);
    
    SignalState previousState = stateMachine->currentState;
    
    switch (stateMachine->currentState) {
        case SIGNAL_STATE_STABLE:
            // Check for transition to changing state
            if (absDerivative > DERIVATIVE_THRESHOLD) {
                stateMachine->currentState = SIGNAL_STATE_CHANGING;
                stateMachine->stateStartTime = currentTime;
                stateMachine->transitionPending = false;
                stateChanged = true;
                Log::debug("[Telemetry] State: STABLE → CHANGING (derivative=%.1f)", currentDerivative);
            }
            break;
            
        case SIGNAL_STATE_CHANGING:
            // Check for transition back to stable (with hysteresis)
            if (absDerivative < (DERIVATIVE_THRESHOLD * EVENT_HYSTERESIS_FACTOR)) {
                // Require minimum time in changing state before allowing transition
                if ((currentTime - stateMachine->stateStartTime) >= MIN_EVENT_DURATION_MS) {
                    stateMachine->currentState = SIGNAL_STATE_STABLE;
                    stateMachine->stateStartTime = currentTime;
                    stateMachine->transitionPending = false;
                    stateChanged = true;
                    Log::debug("[Telemetry] State: CHANGING → STABLE (derivative=%.1f)", currentDerivative);
                }
            }
            break;
    }
    
    if (stateChanged) {
        stateMachine->lastEventTime = currentTime;
        stateMachine->eventsDetected++;
    }
    
    return stateChanged;
}

/**
 * @brief Processes a sample during stable period.
 */
bool processStablePeriod(StableAccumulator* accumulator, uint32_t filteredValue, uint64_t timestamp) {
    // Initialize accumulator if this is the first sample
    if (accumulator->sampleCount == 0) {
        accumulator->minValue = filteredValue;
        accumulator->maxValue = filteredValue;
        accumulator->sumValues = 0;
        accumulator->periodStartTime = timestamp;
    }
    
    // Update statistics
    accumulator->minValue = min(accumulator->minValue, filteredValue);
    accumulator->maxValue = max(accumulator->maxValue, filteredValue);
    accumulator->sumValues += filteredValue;
    accumulator->sampleCount++;
    
    // Check if stable period should be finalized
    uint64_t periodDuration = timestamp - accumulator->periodStartTime;
    
    // Finalize if minimum duration reached and we have enough samples
    if (periodDuration >= MIN_STABLE_DURATION_MS && accumulator->sampleCount >= 50) {
        return true;
    }
    
    // Force finalization if maximum timeout reached
    if (periodDuration >= MAX_INTERVAL_TIMEOUT_MS) {
        return true;
    }
    
    return false;
}

/**
 * @brief Processes a sample during changing period.
 */
bool processChangingPeriod(PressureEvent* event, uint32_t filteredValue, uint64_t timestamp, float derivative) {
    // Initialize event if this is the first sample
    if (event->sampleCount == 0) {
        event->startTimestamp = timestamp;
        event->startValue = filteredValue;
        event->hasDetailedSamples = true;
        event->triggerReason = (derivative > 0) ? TRIGGER_DERIVATIVE_RISING : TRIGGER_DERIVATIVE_FALLING;
    }
    
    // Add sample to detailed array if there's space
    if (event->sampleCount < MAX_SAMPLES_PER_EVENT) {
        event->samples[event->sampleCount].timestamp = timestamp;
        event->samples[event->sampleCount].filteredValue = filteredValue;
        event->samples[event->sampleCount].derivative = derivative;
    }
    
    // Update event properties
    event->endTimestamp = timestamp;
    event->endValue = filteredValue;
    event->sampleCount++;
    
    // Check if event should be finalized
    uint64_t eventDuration = timestamp - event->startTimestamp;
    
    // Finalize if we've collected enough samples or reached time limit
    if (event->sampleCount >= MAX_SAMPLES_PER_EVENT || eventDuration >= MAX_INTERVAL_TIMEOUT_MS) {
        return true;
    }
    
    return false;
}

/**
 * @brief Finalizes and sends a stable period as an event.
 */
void finalizeStablePeriod(const StableAccumulator* accumulator, uint64_t endTimestamp) {
    if (accumulator->sampleCount == 0) {
        return;
    }
    
    PressureEvent event = {0};
    event.startTimestamp = accumulator->periodStartTime;
    event.endTimestamp = endTimestamp;
    event.type = EVENT_TYPE_STABLE;
    event.startValue = accumulator->sumValues / accumulator->sampleCount;  // Average
    event.endValue = event.startValue;  // For stable periods, start = end
    event.sampleCount = accumulator->sampleCount;
    event.triggerReason = TRIGGER_TIMEOUT;
    event.hasDetailedSamples = false;  // Only statistics for stable periods
    
    // Send to event queue
    if (xQueueSend(g_pressureEventQueue, &event, 0) == pdTRUE) {
        stablePeriodsDetected++;
        Log::debug("[Telemetry] ✓ Stable period: %llu-%llu ms, avg=%lu, samples=%u", 
                   event.startTimestamp, event.endTimestamp, event.startValue, event.sampleCount);
    } else {
        eventsQueueFull++;
        Log::warn("[Telemetry] Event queue full - dropped stable period");
        notifySystemState(EVENT_PRESSURE_QUEUE_FULL);
    }
}

/**
 * @brief Finalizes and sends a changing period as an event.
 */
void finalizeChangingEvent(PressureEvent* event) {
    if (event->sampleCount == 0) {
        return;
    }
    
    // Calculate average derivative for classification
    float avgDerivative = 0.0f;
    if (event->hasDetailedSamples && event->sampleCount > 1) {
        uint16_t maxSamples = min((uint16_t)event->sampleCount, (uint16_t)MAX_SAMPLES_PER_EVENT);
        for (uint16_t i = 0; i < maxSamples; i++) {
            avgDerivative += event->samples[i].derivative;
        }
        avgDerivative /= maxSamples;
    }
    
    // Classify event type
    event->type = classifyEvent(avgDerivative, event->startValue, event->endValue);
    
    // Send to event queue
    if (xQueueSend(g_pressureEventQueue, event, 0) == pdTRUE) {
        changingEventsDetected++;
        Log::debug("[Telemetry] ✓ Change event: %s, %llu-%llu ms, %lu→%lu, samples=%u", 
                   getEventTypeString(event->type), event->startTimestamp, event->endTimestamp,
                   event->startValue, event->endValue, event->sampleCount);
    } else {
        eventsQueueFull++;
        Log::warn("[Telemetry] Event queue full - dropped changing event");
        notifySystemState(EVENT_PRESSURE_QUEUE_FULL);
    }
}

/**
 * @brief Main telemetry processing task.
 */
void pressureTelemetryTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t processInterval = pdMS_TO_TICKS(TELEMETRY_PROCESS_INTERVAL_MS);
    
    Log::info("[Telemetry] Signal processing task started");
    
    while (1) {
        uint64_t cycleStartTime = millis();
        
        // Process all available samples from pressure reader
        PressureReading reading;
        while (xQueueReceive(g_pressureQueue, &reading, 0) == pdTRUE) {
            // Skip invalid readings
            if (!reading.isValid) {
                continue;
            }
            
            totalSamplesProcessed++;
            float rawFloat = (float)reading.rawValue;

            // Apply double EPA filtering
            if (!filtersInitialized) {
                primaryFiltered = rawFloat;
                secondaryFiltered = rawFloat;
                filtersInitialized = true;
                
                // Initialize state machine
                stateMachine.stateStartTime = reading.timestamp;
                Log::debug("[Telemetry] EPA filters initialized with value: %.0f", rawFloat);
                continue;
            }

            // Apply cascaded EPA filters
            primaryFiltered = applyEPAFilter(rawFloat, primaryFiltered, EPA_ALPHA_PRIMARY);
            secondaryFiltered = applyEPAFilter(primaryFiltered, secondaryFiltered, EPA_ALPHA_SECONDARY);
            uint32_t filteredValue = (uint32_t)secondaryFiltered;

            // Add to derivative window and calculate derivative
            addToDerivativeWindow(&derivativeWindow, secondaryFiltered, reading.timestamp);
            float currentDerivative = calculateDerivative(&derivativeWindow);
            
            // Apply smoothing to derivative
            filteredDerivative = applyEPAFilter(currentDerivative, filteredDerivative, DERIVATIVE_FILTER_ALPHA);

            // Update state machine
            bool stateChanged = updateSignalStateMachine(&stateMachine, filteredDerivative, reading.timestamp);

            // Process sample based on current state
            if (stateMachine.currentState == SIGNAL_STATE_STABLE) {
                // If we just transitioned from changing to stable, finalize the changing event
                if (stateChanged && eventInProgress) {
                    finalizeChangingEvent(&currentEvent);
                    memset(&currentEvent, 0, sizeof(PressureEvent));
                    eventInProgress = false;
                }

                // Process stable period
                bool shouldFinalize = processStablePeriod(&stableAccumulator, filteredValue, reading.timestamp);
                if (shouldFinalize) {
                    finalizeStablePeriod(&stableAccumulator, reading.timestamp);
                    memset(&stableAccumulator, 0, sizeof(StableAccumulator));
                }
                
            } else { // SIGNAL_STATE_CHANGING
                // If we just transitioned from stable to changing, finalize the stable period
                if (stateChanged && stableAccumulator.sampleCount > 0) {
                    finalizeStablePeriod(&stableAccumulator, reading.timestamp);
                    memset(&stableAccumulator, 0, sizeof(StableAccumulator));
                }

                // Process changing period
                if (!eventInProgress) {
                    memset(&currentEvent, 0, sizeof(PressureEvent));
                    eventInProgress = true;
                }
                
                bool shouldFinalize = processChangingPeriod(&currentEvent, filteredValue, reading.timestamp, filteredDerivative);
                if (shouldFinalize) {
                    finalizeChangingEvent(&currentEvent);
                    memset(&currentEvent, 0, sizeof(PressureEvent));
                    eventInProgress = false;
                }
            }
        }

        // Periodic statistics (every 30 seconds)
        static uint64_t lastStatsTime = 0;
        if (cycleStartTime - lastStatsTime > 30000) {
            lastStatsTime = cycleStartTime;
            
            UBaseType_t pressureQueueLevel = uxQueueMessagesWaiting(g_pressureQueue);
            UBaseType_t eventQueueLevel = uxQueueMessagesWaiting(g_pressureEventQueue);
            
            Log::info("[Telemetry] Signal processing stats:");
            Log::info("  Samples processed: %lu, stable periods: %lu, changing events: %lu", 
                      totalSamplesProcessed, stablePeriodsDetected, changingEventsDetected);
            Log::info("  Current state: %s, derivative: %.1f, queue levels: pressure=%lu/%d, events=%lu/%d",
                      (stateMachine.currentState == SIGNAL_STATE_STABLE) ? "STABLE" : "CHANGING",
                      filteredDerivative, pressureQueueLevel, PRESSURE_QUEUE_SIZE,
                      eventQueueLevel, PRESSURE_EVENT_QUEUE_SIZE);
            
            if (eventsQueueFull > 0) {
                Log::warn("  Events dropped due to queue full: %lu", eventsQueueFull);
            }
        }

        // Wait for next processing cycle
        vTaskDelayUntil(&lastWakeTime, processInterval);
    }
}

