/*
 * message_formatter.cpp
 *
 * Message formatter module for pressure events.
 * Reads pressure events from g_pressureEventQueue, formats them into optimized
 * JSON messages, and sends them to g_mqttQueue for MQTT transmission.
 *
 * Key Features:
 * - Intelligent batching of events to optimize bandwidth
 * - Different JSON formats for stable vs changing events
 * - Compression and optimization of JSON payload
 * - Tolerance to queue overflows with intelligent dropping
 * 
 * Input: g_pressureEventQueue (pressure events from telemetry)
 * Output: g_mqttQueue (formatted MQTT messages with JSON payload)
 */

#include "message_formatter.h"
#include "device_id.h"
#include "data_types.h"
#include "signal_parameters.h"
#include "system_state.h"
#include <ArduinoJson.h>

// Global queue (created here, declared in data_types.h)
QueueHandle_t g_mqttQueue = NULL;

// Event batching buffer
static PressureEvent eventBatchBuffer[MAX_EVENTS_PER_MESSAGE];
static uint8_t batchEventCount = 0;
static uint64_t lastBatchSendTime = 0;

// Statistics
static uint32_t totalEventsProcessed = 0;
static uint32_t messagesFormatted = 0;
static uint32_t jsonErrors = 0;

/**
 * @brief Initializes the message formatter system.
 */
bool initializeMessageFormatter() {
    // Create MQTT message queue
    g_mqttQueue = xQueueCreate(MQTT_QUEUE_SIZE, sizeof(MqttMessage));
    if (g_mqttQueue == NULL) {
        Log::error("[Formatter] Failed to create MQTT queue");
        return false;
    }
    
    // Initialize batching state
    batchEventCount = 0;
    lastBatchSendTime = millis();
    
    // Initialize statistics
    totalEventsProcessed = 0;
    messagesFormatted = 0;
    jsonErrors = 0;

    Log::info("[Formatter] Initialized - MQTT queue size: %d, max events per message: %d, timeout: %dms", 
              MQTT_QUEUE_SIZE, MAX_EVENTS_PER_MESSAGE, FORMATTER_SEND_TIMEOUT_MS);
    return true;
}

/**
 * @brief Formats a single pressure event to JSON object.
 */
void formatEventToJson(const PressureEvent* event, JsonObject& jsonObject) {
    jsonObject["type"] = getEventTypeString(event->type);
    jsonObject["startTimestamp"] = event->startTimestamp;
    jsonObject["endTimestamp"] = event->endTimestamp;
    jsonObject["sampleCount"] = event->sampleCount;
    jsonObject["duration_ms"] = event->endTimestamp - event->startTimestamp;
    
    if (event->type == EVENT_TYPE_STABLE) {
        // For stable events, only include average pressure
        uint32_t averagePressure = (event->startValue + event->endValue) / 2;
        jsonObject["pressure"] = averagePressure;
    } else {
        // For changing events, include start/end values and trigger reason
        jsonObject["startValue"] = event->startValue;
        jsonObject["endValue"] = event->endValue;
        jsonObject["triggerReason"] = getTriggerReasonString(event->triggerReason);
        
        // Include detailed samples if available and not too many
        if (event->hasDetailedSamples && event->sampleCount <= 50) {
            JsonArray samples = jsonObject.createNestedArray("samples");
            for (uint16_t i = 0; i < event->sampleCount && i < MAX_SAMPLES_PER_EVENT; i++) {
                JsonArray sample = samples.createNestedArray();
                sample.add(event->samples[i].timestamp);
                sample.add(event->samples[i].filteredValue);
            }
        }
    }
}

/**
 * @brief Calculates optimal batch size based on event types and sizes.
 */
uint8_t calculateOptimalBatchSize(const PressureEvent* events, uint8_t eventCount) {
    uint32_t estimatedJsonSize = 200;  // Base JSON overhead
    
    for (uint8_t i = 0; i < eventCount; i++) {
        if (events[i].type == EVENT_TYPE_STABLE) {
            estimatedJsonSize += 150;  // ~150 bytes for stable event
        } else {
            // Changing event with samples
            uint32_t sampleBytes = events[i].hasDetailedSamples ? 
                                   (events[i].sampleCount * 25) : 0;  // ~25 bytes per sample
            estimatedJsonSize += 200 + sampleBytes;
        }
        
        // Stop if we would exceed MQTT payload limit (leave 512 bytes margin)
        if (estimatedJsonSize > 3584) {  // 4096 - 512 = 3584
            return i;
        }
    }
    
    return eventCount;
}

/**
 * @brief Formats batch of events into JSON and sends to MQTT queue.
 */
static void sendEventBatchToMqtt() {
    if (batchEventCount == 0) {
        return;
    }

    // Create MQTT message
    MqttMessage mqttMsg;
    mqttMsg.qos = 0;  // QoS 0 for telemetry

    // Build topic
    String deviceId = getDeviceId();
    snprintf(mqttMsg.topic, sizeof(mqttMsg.topic), 
             "mica/dev/telemetry/gateway/%s/pressure-events", deviceId.c_str());

    // Build JSON payload
    DynamicJsonDocument doc(4096);
    doc["sensor_id"] = deviceId;
    doc["sentTimestamp"] = millis();
    
    JsonArray events = doc.createNestedArray("events");
    for (uint8_t i = 0; i < batchEventCount; i++) {
        JsonObject event = events.createNestedObject();
        formatEventToJson(&eventBatchBuffer[i], event);
    }

    // Serialize to string
    size_t jsonSize = serializeJson(doc, mqttMsg.payload, sizeof(mqttMsg.payload));
    
    if (jsonSize == 0) {
        Log::error("[Formatter] Failed to serialize JSON");
        jsonErrors++;
        return;
    }
    
    if (jsonSize >= sizeof(mqttMsg.payload) - 1) {
        Log::warn("[Formatter] JSON payload truncated (%d bytes)", jsonSize);
    }

    // Log sample of JSON payload
    Log::debug("[Formatter] JSON payload (%d bytes, %d events)", jsonSize, batchEventCount);

    // Send to MQTT queue
    if (xQueueSend(g_mqttQueue, &mqttMsg, pdMS_TO_TICKS(100)) == pdTRUE) {
        messagesFormatted++;
        Log::info("[Formatter] ✓ Sent %d events to MQTT queue (%d bytes)", 
                  batchEventCount, jsonSize);
    } else {
        Log::warn("[Formatter] MQTT queue full, message dropped");
    }

    // Reset batch
    batchEventCount = 0;
    lastBatchSendTime = millis();
}

/**
 * @brief Main message formatter processing task.
 */
void messageFormatterTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t processInterval = pdMS_TO_TICKS(FORMATTER_PROCESS_INTERVAL_MS);
    
    Log::info("[Formatter] Task started");
    
    while (1) {
        // Safety check: Only process if MQTT is connected
        SystemState currentState = getSystemState();
        if (currentState != SYSTEM_STATE_CONNECTED_MQTT) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        uint64_t currentTime = millis();
        
        // Process available events in queue
        PressureEvent event;
        while (xQueueReceive(g_pressureEventQueue, &event, 0) == pdTRUE) {
            totalEventsProcessed++;
            
            // Add to batch buffer
            if (batchEventCount < MAX_EVENTS_PER_MESSAGE) {
                eventBatchBuffer[batchEventCount++] = event;
                
                Log::debug("[Formatter] Added %s event to batch (%d/%d)", 
                          getEventTypeString(event.type), batchEventCount, MAX_EVENTS_PER_MESSAGE);
            } else {
                Log::warn("[Formatter] Event batch buffer full, dropping event");
            }
        }

        // Check if we should send batch
        bool timeoutReached = (currentTime - lastBatchSendTime) >= FORMATTER_SEND_TIMEOUT_MS;
        bool batchFull = (batchEventCount >= MAX_EVENTS_PER_MESSAGE);

        if ((timeoutReached || batchFull) && batchEventCount > 0) {
            sendEventBatchToMqtt();
        }

        // Periodic statistics (every 30 seconds)
        static uint64_t lastStatsTime = 0;
        if (currentTime - lastStatsTime > 30000) {
            lastStatsTime = currentTime;
            
            UBaseType_t eventQueueLevel = uxQueueMessagesWaiting(g_pressureEventQueue);
            UBaseType_t mqttQueueLevel = uxQueueMessagesWaiting(g_mqttQueue);
            
            Log::info("[Formatter] Stats: events=%lu, messages=%lu, errors=%lu, queues: events=%lu/%d, mqtt=%lu/%d",
                      totalEventsProcessed, messagesFormatted, jsonErrors,
                      eventQueueLevel, PRESSURE_EVENT_QUEUE_SIZE,
                      mqttQueueLevel, MQTT_QUEUE_SIZE);
        }

        // Wait for next processing cycle
        vTaskDelayUntil(&lastWakeTime, processInterval);
    }
}