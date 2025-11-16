#ifndef MESSAGE_FORMATTER_H
#define MESSAGE_FORMATTER_H

#include <Arduino.h>
#include "data_types.h"      // For PressureEvent and MqttMessage structures
#include "signal_parameters.h"
#include "Log.h"
#include <ArduinoJson.h>     // For JsonObject

// Message Formatter Module
// Purpose:
// - Reads PressureEvent structures from g_pressureEventQueue
// - Groups events into optimal batches for JSON serialization
// - Formats different JSON structures for stable vs changing events
// - Sends formatted MqttMessage structures to g_mqttQueue
// - Optimizes bandwidth by batching and compressing events

/**
 * @brief Initializes the message formatter system.
 * Creates any internal resources needed for JSON formatting.
 * @return true if initialization successful, false otherwise.
 */
bool initializeMessageFormatter();

/**
 * @brief FreeRTOS task for message formatting and batching.
 * Reads from g_pressureEventQueue, groups events into optimal batches,
 * serializes to JSON according to event type, and sends to g_mqttQueue.
 * @param pvParameters Task parameters (not used).
 */
void messageFormatterTask(void *pvParameters);

/**
 * @brief Formats a single pressure event to JSON object.
 * Creates optimized JSON structure based on event type.
 * @param event The pressure event to format
 * @param jsonObject The JSON object to populate
 */
void formatEventToJson(const PressureEvent* event, JsonObject& jsonObject);

/**
 * @brief Calculates optimal batch size based on event types and sizes.
 * Ensures JSON payload stays within MQTT limits.
 * @param events Array of events to analyze
 * @param eventCount Number of events in array
 * @return Optimal number of events to include in batch
 */
uint8_t calculateOptimalBatchSize(const PressureEvent* events, uint8_t eventCount);

#endif // MESSAGE_FORMATTER_H