// wifi_connect.h
#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "includes.h"
#include "eeprom_config.h"

// WiFi Connect Module
// Purpose:
// Manages the connection to a WiFi network using credentials stored in EEPROM.

/**
 * @brief Initializes the WiFi connection in station mode.
 * @return true if initialization is successful, false otherwise.
 */
bool initializeWiFiConnection();

/**
 * @brief FreeRTOS task to handle WiFi connection.
 * @param pvParameters Task parameters (not used).
 */
void wifiConnectTask(void *pvParameters);

#endif