
#ifndef PRESSURE_TELEMETRY_H
#define PRESSURE_TELEMETRY_H

#include <Arduino.h>
#include "pressure_reader.h"
#include "signal_parameters.h"
#include "Log.h"

// Structure for adaptive pressure intervals
typedef struct {
    uint64_t startTimestamp;       // Interval start time
    uint64_t endTimestamp;         // Interval end time
    uint32_t pressure;             // Representative pressure value (filtered average)
    uint16_t samplesUsed;          // Number of samples accumulated in this interval
} PressureInterval;

// Structure for MQTT messages (generic message queue)
typedef struct {
    char topic[128];               // MQTT topic
    char payload[4096];            // JSON payload
    uint8_t qos;                   // QoS level (0 or 1)
} MqttMessage;

// External queue references
extern QueueHandle_t g_pressureQueue;       // Input: raw pressure readings
extern QueueHandle_t g_mqttQueue;           // Output: formatted MQTT messages

/**
 * @brief Initializes the pressure telemetry system.
 * Creates MQTT queue and initializes filtering state.
 * @return true if initialization successful, false otherwise.
 */
bool initializePressureTelemetry();

/**
 * @brief FreeRTOS task for pressure telemetry processing.
 * Reads from g_pressureQueue, applies EPA filtering, detects changes,
 * groups into adaptive intervals, and sends to g_mqttQueue.
 * @param pvParameters Task parameters (not used).
 */
void pressureTelemetryTask(void *pvParameters);

#endif // PRESSURE_TELEMETRY_H
