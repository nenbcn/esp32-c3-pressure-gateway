// system_state.h
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "includes.h"
#include "Log.h"

// System State Management Module
// Purpose:
// Manages the global system state, coordinates state transitions, and activates/deactivates tasks accordingly.

// System States
typedef enum {
    SYSTEM_STATE_CONNECTING,             // El sistema está intentando conectarse (estado inicial)
    SYSTEM_STATE_CONNECTED_WIFI,         // Conectado a WiFi pero no a MQTT
    SYSTEM_STATE_CONFIG_MQTT,           // Conectado a WiFi y configurando MQTT 
    SYSTEM_STATE_CONNECTED_MQTT,         // Conectado a WiFi y MQTT
    SYSTEM_STATE_CONFIG_MODE,            // Modo de configuración activado
    SYSTEM_STATE_OTA_UPDATE,             // OTA update state
    SYSTEM_STATE_ERROR                   // Error crítico detectado
} SystemState;

// Task Notification Events
typedef enum {
    EVENT_WIFI_CONNECTED        = (1 << 0),  // WiFi connected successfully
    EVENT_MQTT_CONNECTED        = (1 << 1),  // MQTT connected successfully
    EVENT_WIFI_FAIL_CONNECT     = (1 << 2),  // Failed to connect to WiFi
    EVENT_NO_PARAMETERS_EEPROM  = (1 << 3),  // No WiFi parameters in EEPROM
    EVENT_LORA_ERROR            = (1 << 4),  // Error in LoRa module
    EVENT_LORA_DATA_RECEIVED    = (1 << 5),  // LoRa data received
    EVENT_LORA_QUEUE_FULL       = (1 << 6),  // LoRa queue full
    EVENT_WIFI_CONFIG_STARTED   = (1 << 7),  // WiFi configuration mode started
    EVENT_WIFI_CONFIG_FAILED    = (1 << 8),  // Failed to configure WiFi
    EVENT_WIFI_CONFIG_SAVED     = (1 << 9),  // WiFi configuration saved
    EVENT_WIFI_CONFIG_STOPPED   = (1 << 10), // WiFi configuration mode stopped
    EVENT_MQTT_DISCONNECTED     = (1 << 11), // MQTT disconnected
    EVENT_LONG_PRESS_BUTTON     = (1 << 12), // Long button press (5 seconds)
    EVENT_WIFI_DISCONNECTED     = (1 << 15), // WiFi disconnected
    EVENT_OTA_UPDATE            = (1 << 16), // OTA update event
    EVENT_MQTT_AWS_CREDENTIALS  = (1 << 17), // AWS credentials received
    EVENT_PRESSURE_QUEUE_FULL   = (1 << 18), // Pressure queue full
    EVENT_I2C_ERROR_RECOVERY    = (1 << 19)  // I2C bus reset after errors
} TaskNotificationEvent;

/**
 * @brief Initializes the system state and creates necessary tasks.
 * @return true if initialization is successful, false otherwise.
 */
bool initializeSystemState();

/**
 * @brief Sets the new system state safely.
 * @param state New state to set.
 */
void setSystemState(SystemState state);

/**
 * @brief Gets the current system state safely.
 * @return Current system state.
 */
SystemState getSystemState();

/**
 * @brief Notifies the system of an event to handle state transitions.
 * @param event Event to notify (TaskNotificationEvent).
 */
void notifySystemState(TaskNotificationEvent event);

/**
 * @brief Sets the OTA Task Handle.
 * @param handle The OTA task handle, or NULL if no OTA task is running.
 */
void setOtaTaskHandle(TaskHandle_t handle);

#endif