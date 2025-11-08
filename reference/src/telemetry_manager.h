// telemetry_manager.h
#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include "includes.h"
#include "mqtt_handler.h"

// Telemetry Manager Module
// Purpose:
// Handles the buffering, scheduling, and transmission of telemetry data
// including pulse data and health checks to cloud services

// Constants for telemetry management
#define BUFFER_SIZE 64               // Maximum number of processed data entries to buffer // TODO: Set correct SIZE
#define INACTIVITY_TIMEOUT 2000      // Timeout in ms to consider pulse train completed
#define HEALTHCHECK_INTERVAL 60000   // Send health check every minute // TODO: Change to every hour for production

// Send reason enums
enum SendReason {
    BUFFER_FULL = 1,  // Buffer is full
    TIMEOUT = 2,      // Inactivity timeout reached
    HEALTHCHECK = 3   // Regular health check message
};

/**
 * @brief Initializes the telemetry manager.
 * @return true if initialization is successful, false otherwise.
 */
bool initializeTelemetryManager();

/**
 * @brief Adds processed pulse data to the telemetry buffer
 * @param data The processed data entry to add
 * @return true if buffer is full after adding, false otherwise
 */
bool addPulseDataToBuffer(const ProcessedData& data);

/**
 * @brief Sends buffered pulse data via MQTT
 * @param reason Reason for sending (BUFFER_FULL or TIMEOUT)
 * @return true if data was sent successfully, false otherwise
 */
bool sendBufferedPulseData(SendReason reason);

/**
 * @brief Get current time in milliseconds with NTP synchronization if available.
 * @param millisTimestamp Optional timestamp in millis() to convert to Unix time. If 0, uses current millis.
 * @return Current time as Unix timestamp in milliseconds or uptime if not synchronized.
 */
uint64_t getCurrentTime(uint64_t millisTimestamp = 0);

/**
 * @brief FreeRTOS task to handle telemetry transmissions
 * @param pvParameters Task parameters (not used)
 */
void telemetryTask(void *pvParameters);

#endif // TELEMETRY_MANAGER_H 