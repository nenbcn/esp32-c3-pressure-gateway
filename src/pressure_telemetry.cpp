/*
 * pressure_telemetry.cpp
 *
 * Simplified pressure telemetry module for real-time monitoring.
 * Processes pressure readings, applies EPA filtering, detects significant changes,
 * and groups data into adaptive intervals for efficient MQTT transmission.
 *
 * Key Features:
 * - Double EPA filtering for noise reduction
 * - Adaptive interval creation (compress stable periods, preserve changes)
 * - 1-second timeout for real-time graphing
 * - Accumulates up to 20 intervals per MQTT message
 * 
 * Input: g_pressureQueue (raw readings from pressure_reader)
 * Output: g_mqttQueue (formatted MQTT messages with JSON payload)
 */

#include "pressure_telemetry.h"
#include "device_id.h"
#include "led_manager.h"
#include <ArduinoJson.h>

// Global MQTT queue
QueueHandle_t g_mqttQueue = NULL;

// EPA filter state
static float primaryFiltered = 0.0f;
static float secondaryFiltered = 0.0f;
static bool filtersInitialized = false;

// Current interval being accumulated
static PressureInterval currentInterval;
static bool intervalActive = false;
static uint32_t samplesInInterval = 0;
static float intervalPressureSum = 0.0f;

// Accumulated intervals buffer (for grouping into messages)
static PressureInterval intervalBuffer[MAX_INTERVALS_PER_MESSAGE];
static uint8_t intervalCount = 0;
static uint64_t lastSendTime = 0;

// Statistics
static uint32_t totalSamplesProcessed = 0;
static uint32_t intervalsCreated = 0;
static uint32_t messagesSent = 0;

/**
 * @brief Initializes the pressure telemetry system.
 */
bool initializePressureTelemetry() {
    // Create MQTT message queue
    g_mqttQueue = xQueueCreate(MQTT_QUEUE_SIZE, sizeof(MqttMessage));
    if (g_mqttQueue == NULL) {
        Log::error("Failed to create MQTT queue");
        return false;
    }

    // Initialize state
    filtersInitialized = false;
    intervalActive = false;
    intervalCount = 0;
    lastSendTime = millis();
    
    totalSamplesProcessed = 0;
    intervalsCreated = 0;
    messagesSent = 0;

    Log::info("[Telemetry] Initialized with %dms process interval, %dms send timeout", 
              TELEMETRY_PROCESS_INTERVAL_MS, TELEMETRY_SEND_TIMEOUT_MS);
    return true;
}

/**
 * @brief Applies EPA (Exponential Moving Average) filter.
 */
static inline float applyEPAFilter(float newValue, float prevFiltered, float alpha) {
    return alpha * newValue + (1.0f - alpha) * prevFiltered;
}

/**
 * @brief Detects if pressure change is significant enough to close current interval.
 */
static bool isSignificantChange(float currentFiltered, float intervalAverage) {
    // Absolute threshold
    float absoluteDiff = fabs(currentFiltered - intervalAverage);
    if (absoluteDiff > PRESSURE_CHANGE_THRESHOLD) {
        return true;
    }
    
    // Relative threshold
    if (intervalAverage > 0) {
        float relativeDiff = absoluteDiff / intervalAverage;
        if (relativeDiff > (PRESSURE_CHANGE_PERCENT / 100.0f)) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Closes current interval and adds it to the buffer.
 * @param endTimestamp Timestamp when the interval ends
 * @param wasSignificantChange True if interval closed due to significant pressure change
 */
static void closeCurrentInterval(uint64_t endTimestamp, bool wasSignificantChange = false) {
    if (!intervalActive || samplesInInterval == 0) {
        return;
    }

    // Calculate average pressure for the interval
    currentInterval.endTimestamp = endTimestamp;
    currentInterval.pressure = (uint32_t)(intervalPressureSum / samplesInInterval);
    currentInterval.samplesUsed = samplesInInterval;

    // Add to buffer
    if (intervalCount < MAX_INTERVALS_PER_MESSAGE) {
        intervalBuffer[intervalCount++] = currentInterval;
        intervalsCreated++;
        
        Log::debug("[Telemetry] Interval closed: %llu-%llu, pressure=%lu, samples=%u %s",
                   currentInterval.startTimestamp, currentInterval.endTimestamp,
                   currentInterval.pressure, currentInterval.samplesUsed,
                   wasSignificantChange ? "(significant change)" : "(timeout)");
        
        // Trigger blue LED blink ONLY if closed due to significant pressure change
        if (wasSignificantChange) {
            triggerPressureChangeLed();
        }
    } else {
        Log::warn("[Telemetry] Interval buffer full, dropping interval");
    }

    // Reset interval state
    intervalActive = false;
    samplesInInterval = 0;
    intervalPressureSum = 0.0f;
}

/**
 * @brief Starts a new interval.
 */
static void startNewInterval(uint64_t startTimestamp, float filteredPressure) {
    currentInterval.startTimestamp = startTimestamp;
    currentInterval.endTimestamp = startTimestamp;
    intervalPressureSum = filteredPressure;
    samplesInInterval = 1;
    intervalActive = true;
}

/**
 * @brief Formats intervals into JSON and sends to MQTT queue.
 */
static void sendIntervalsToMqtt() {
    if (intervalCount == 0) {
        return;
    }

    // Create MQTT message
    MqttMessage mqttMsg;
    mqttMsg.qos = 0;  // QoS 0 for telemetry

    // Build topic
    String deviceId = getDeviceId();
    snprintf(mqttMsg.topic, sizeof(mqttMsg.topic), 
             "mica/dev/telemetry/gateway/%s/pressure-data", deviceId.c_str());

    // Build JSON payload (using DynamicJsonDocument to allocate on heap, not stack)
    DynamicJsonDocument doc(4096);
    doc["sensor_id"] = deviceId;
    
    JsonArray intervals = doc.createNestedArray("intervals");
    for (uint8_t i = 0; i < intervalCount; i++) {
        JsonObject interval = intervals.createNestedObject();
        interval["startTimestamp"] = intervalBuffer[i].startTimestamp;
        interval["endTimestamp"] = intervalBuffer[i].endTimestamp;
        interval["pressure"] = intervalBuffer[i].pressure;
        interval["samplesUsed"] = intervalBuffer[i].samplesUsed;
    }

    // Serialize to string
    size_t jsonSize = serializeJson(doc, mqttMsg.payload, sizeof(mqttMsg.payload));
    
    if (jsonSize == 0) {
        Log::error("[Telemetry] Failed to serialize JSON");
        return;
    }

    // Log JSON payload before sending (using Serial.println to avoid truncation)
    Serial.printf("\n[Telemetry] JSON payload (%d bytes):\n", jsonSize);
    Serial.println(mqttMsg.payload);
    Serial.println();

    // Send to MQTT queue
    if (xQueueSend(g_mqttQueue, &mqttMsg, pdMS_TO_TICKS(100)) == pdTRUE) {
        messagesSent++;
        Log::info("[Telemetry] ✓ Sent %d intervals to MQTT queue", intervalCount);
    } else {
        Log::warn("[Telemetry] MQTT queue full, message dropped");
    }

    // Reset buffer
    intervalCount = 0;
    lastSendTime = millis();
}

/**
 * @brief Main telemetry processing task.
 */
void pressureTelemetryTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t processInterval = pdMS_TO_TICKS(TELEMETRY_PROCESS_INTERVAL_MS);
    
    Log::info("[Telemetry] Task started");
    
    while (1) {
        uint64_t cycleStartTime = millis();
        
        // Process all available samples in queue
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
                
                // Start first interval
                startNewInterval(reading.timestamp, secondaryFiltered);
                continue;
            }

            primaryFiltered = applyEPAFilter(rawFloat, primaryFiltered, EPA_ALPHA_PRIMARY);
            secondaryFiltered = applyEPAFilter(primaryFiltered, secondaryFiltered, EPA_ALPHA_SECONDARY);

            // Check if we need to close current interval and start a new one
            if (intervalActive) {
                float intervalAverage = intervalPressureSum / samplesInInterval;
                
                if (isSignificantChange(secondaryFiltered, intervalAverage)) {
                    // Significant change detected - close current interval
                    closeCurrentInterval(reading.timestamp, true);  // ← true = significant change
                    startNewInterval(reading.timestamp, secondaryFiltered);
                } else {
                    // No significant change - accumulate in current interval
                    intervalPressureSum += secondaryFiltered;
                    samplesInInterval++;
                    currentInterval.endTimestamp = reading.timestamp;
                }
            } else {
                // No active interval - start new one
                startNewInterval(reading.timestamp, secondaryFiltered);
            }
        }

        // Check if we should send accumulated intervals
        uint64_t currentTime = millis();
        bool timeoutReached = (currentTime - lastSendTime) >= TELEMETRY_SEND_TIMEOUT_MS;
        bool bufferFull = (intervalCount >= MAX_INTERVALS_PER_MESSAGE);

        if (timeoutReached || bufferFull) {
            // Close current interval if active (false = closed by timeout, not significant change)
            if (intervalActive) {
                closeCurrentInterval(currentTime, false);  // ← false = timeout closure
            }
            
            // Send all accumulated intervals
            if (intervalCount > 0) {
                sendIntervalsToMqtt();
            }
        }

        // Periodic statistics (every 10 seconds)
        static uint64_t lastStatsTime = 0;
        if (currentTime - lastStatsTime > 10000) {
            lastStatsTime = currentTime;
            
            UBaseType_t pressureQueueLevel = uxQueueMessagesWaiting(g_pressureQueue);
            UBaseType_t mqttQueueLevel = uxQueueMessagesWaiting(g_mqttQueue);
            
            Log::info("[Telemetry] Stats: samples=%lu, intervals=%lu, messages=%lu, queues: pressure=%lu/%d, mqtt=%lu/%d",
                      totalSamplesProcessed, intervalsCreated, messagesSent,
                      pressureQueueLevel, PRESSURE_QUEUE_SIZE,
                      mqttQueueLevel, MQTT_QUEUE_SIZE);
        }

        // Wait for next processing cycle
        vTaskDelayUntil(&lastWakeTime, processInterval);
    }
}

