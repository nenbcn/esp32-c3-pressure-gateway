// mqtt_handler.h
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "includes.h"
#include "system_state.h"
#include "config.h"

// MQTT Handler Module
// Purpose:
// Manages secure MQTT communication with AWS IoT, readings data from the pulse
// receiver and publishing it to an MQTT topic.
// Optionally, handles incoming messages via a callback.

/**
 * @brief Initializes the MQTT handler with secure client settings for AWS IoT.
 */
void initializeMQTTHandler();

/**
 * @brief Connects to the MQTT broker with retries, notifying system events.
 * @return true if connection succeeds, false otherwise.
 */
bool connectMQTT();

/**
 * @brief Callback for processing incoming MQTT messages.
 * @param topic The message topic.
 * @param payload The message content.
 * @param length The payload length.
 */
void mqttMessageCallback(char* topic, byte* payload, unsigned int length);

void mqttConnectTask(void *pvParameters);

/**
 * @brief FreeRTOS task that publishes messages to an MQTT topic.
 * @param pvParameters Task parameters (not used).
 */
void mqttPublishTask(void *pvParameters);

/**
 * @brief Formats and publishes pulse sensor data to MQTT.
 * @param data Pointer to the processed pulse data.
 * @param count Number of data entries to publish.
 * @return true if publishing succeeds, false otherwise.
 */
bool publishPulseData(ProcessedData* data, uint8_t count);

/**
 * @brief Publishes a health check message to MQTT.
 * @param uptime Device uptime in milliseconds
 * @return true if publishing succeeds, false otherwise.
 */
bool publishHealthCheck(uint64_t uptime);

#endif