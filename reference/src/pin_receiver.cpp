// pin_receiver.cpp
#include "board.h"
#include "pin_receiver.h"

// Constants
const uint32_t PIN_INACTIVITY_TIMEOUT = 2000;  // Timeout in ms to consider pulse train completed

// Queue for sending data from ISR to task (its elements are millis() timestamps)
static QueueHandle_t pulseQueue = NULL;

/**
 * @brief Interrupt Service Routine for pulse detection.
 */
static void IRAM_ATTR pulseDetectionISR() {
    uint64_t timestamp = (uint64_t)millis();
    
    // Send to queue for processing in task context
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (!xQueueSendFromISR(pulseQueue, &timestamp, &xHigherPriorityTaskWoken)) {
        Log::error("Failed to send pulse data to queue"); // TODO: Track number of overflows
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Initializes the pin receiver with interrupts on pin 35
 * @return true if initialization is successful, false otherwise.
 */
bool initializePinReceiver() {
    // Create queue for pulse timestamps
    pulseQueue = xQueueCreate(64, sizeof(uint64_t)); // TODO: Decide queue size
    if (pulseQueue == NULL) {
        Log::error("Failed to create pulse queue.");
        return false;
    }
    
    // Initialize sensor pin as input
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    
    if (digitalPinToInterrupt(SENSOR_PIN) == NOT_AN_INTERRUPT) {
        Log::info("Pin %d is not a valid interrupt pin.", SENSOR_PIN);
        return false;
    }

    // Attach interrupt to pin (RISING edge)
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseDetectionISR, RISING);
    
    Log::info("Pin receiver initialized on pin %d.", SENSOR_PIN);
    return true;
}

/**
 * @brief Helper function to process and send pulse data
 * @param firstPulseTimestamp First pulse timestamp (ms since boot)
 * @param lastPulseTimestamp Last pulse timestamp (ms since boot)
 * @param pulseCount Number of pulses in the group
 * @param periodSum Sum of periods between pulses (ms)
 * @param inactivity Indicates if processing due to inactivity timeout
 * @return true if successfully added to buffer, false otherwise
 */
static bool processPulseData(uint64_t firstPulseTimestamp, uint64_t lastPulseTimestamp, 
                             uint64_t pulseCount, uint64_t periodSum, bool inactivity) {
    ProcessedData data;
    data.startTimestamp = getCurrentTime(firstPulseTimestamp);
    data.endTimestamp = getCurrentTime(lastPulseTimestamp);
    data.pulseCount = pulseCount;
    data.averagePeriod = periodSum / (pulseCount - 1);

    Log::debug("Processing pulse data: start=%llu, end=%llu, count=%llu, avgPeriod=%llu, inactivity=%d",
               data.startTimestamp, data.endTimestamp, data.pulseCount, data.averagePeriod, inactivity);
    
    // Add to buffer via telemetry manager
    bool success = addPulseDataToBuffer(data);
    return success;
}

void pinReceiverTask(void *pvParameters) {
    // Variables persisting across iterations
    static uint64_t lastTimestamp = 0;           // Last pulse processed (ms)
    static uint64_t lastActivityTime = 0;        // Last activity time (ms)
    static uint64_t periodAnterior = 0;          // Previous period (ms)
    static uint64_t pulseCount = 0;              // Pulses in current group
    static uint64_t periodSum = 0;               // Sum of periods in group (ms)
    static uint64_t firstPulseTimestamp = 0;     // Start time of current group (ms)

    // Local variables
    uint64_t timestamp;                          // Received pulse timestamp (ms)
    uint64_t period;                             // Current period (ms)
    uint64_t tolerance;                          // Period tolerance (ms)

    while (true) {
        // Wait for a pulse from the queue (timeout for inactivity check)
        if (xQueueReceive(pulseQueue, &timestamp, pdMS_TO_TICKS(50)) == pdTRUE) {
            lastActivityTime = millis();
            
            // First pulse of a sequence
            if (firstPulseTimestamp == 0) {
                firstPulseTimestamp = timestamp;
                lastTimestamp = timestamp;
                pulseCount = 1;
                periodSum = 0;
                periodAnterior = 0;
                Log::debug("Detected first pulse in sequence: ts=%llu", timestamp);
                continue;
            }

            period = timestamp - lastTimestamp;
            // Calculate tolerance (12.5% of previous period)
            tolerance = (periodAnterior > 0) ? (periodAnterior >> 3) : PIN_INACTIVITY_TIMEOUT; // tolerance = max(periodAnterior >> 3, MIN_TOLERANCE);
            // If period differs significantly, end current group
            uint64_t diff = (period > periodAnterior) ? (period - periodAnterior) : (periodAnterior - period);
            Log::debug("Pulse detected: ts=%llu, period=%llu, periodAnterior=%llu, diff=%llu, tolerance=%llu", 
                       timestamp, period, periodAnterior, diff, tolerance);

            if (diff > tolerance) {
                if (pulseCount > 1) {
                    processPulseData(firstPulseTimestamp, lastTimestamp, pulseCount, periodSum, false);

                    // Start new group
                    firstPulseTimestamp = lastTimestamp;
                    pulseCount = 2;
                    periodSum = period;
                }
            } else {
                // Add to current group
                pulseCount++;
                periodSum += period;
                Log::debug("Pulse added to current sequence: count=%llu", pulseCount);
            }
            // Update for next iteration
            periodAnterior = period;
            lastTimestamp = timestamp;
        } else {
            // No pulse received, check for inactivity
            if ((millis() - lastActivityTime) >= PIN_INACTIVITY_TIMEOUT && firstPulseTimestamp > 0) {
                // Inactivity detected, create final event if we have accumulated pulses
                if (pulseCount > 1) {
                    processPulseData(firstPulseTimestamp, lastTimestamp, pulseCount, periodSum, true);
                }

                // Reset state
                firstPulseTimestamp = 0;
                lastTimestamp = 0;
                pulseCount = 0;
                periodSum = 0;
                periodAnterior = 0;
            }
        }
        vTaskDelay(1); // Yield to other tasks
    }
}