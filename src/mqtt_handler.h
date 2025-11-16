// mqtt_handler.h
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include "data_types.h"         // For MqttMessage structure
#include "Log.h"

// MQTT Handler Module - Simplified Generic Version
// Purpose:
// - Manages MQTT connection to AWS IoT
// - Provisions device credentials (certificate and private key)
// - Publishes generic messages from g_mqttQueue (topic + payload)
// - Independent of message content/format

/**
 * @brief Initializes MQTT handler (must be called after WiFi is connected).
 * @return true if initialization successful, false otherwise.
 */
bool initializeMQTTHandler();

/**
 * @brief FreeRTOS task to provision AWS IoT credentials.
 * Loads from flash or requests from API if not available.
 * @param pvParameters Task parameters (not used).
 */
void mqttConnectTask(void *pvParameters);

/**
 * @brief FreeRTOS task to maintain MQTT connection and publish messages.
 * Reads from g_mqttQueue and publishes generic messages.
 * @param pvParameters Task parameters (not used).
 */
void mqttPublishTask(void *pvParameters);

/**
 * @brief Checks if MQTT client is connected.
 * @return true if connected, false otherwise.
 */
bool isMqttConnected();

#endif // MQTT_HANDLER_H
