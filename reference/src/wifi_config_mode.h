// wifi_config_mode.h
#ifndef WIFI_CONFIG_MODE_H
#define WIFI_CONFIG_MODE_H

#include "includes.h"
#include "system_state.h"
#include "eeprom_config.h"

// WiFi Configuration Mode Module
// Purpose:
// Manages the AP mode and web server to collect Wi-Fi credentials and store them in EEPROM.

/**
 * @brief Initializes the WiFi configuration mode by setting up the AP and web server.
 */
void initializeWiFiConfigMode();

/**
 * @brief Deactivates the WiFi configuration mode by stopping the AP and web server.
 */
void deactivateWiFiConfigMode();

/**
 * @brief FreeRTOS task to handle WiFi configuration.
 * @param pvParameters Task parameters (not used).
 */
void wifiConfigModeTask(void *pvParameters);

// Function to generate HTML options for available Wi-Fi networks // TODO: Implement?
String generateWiFiOptions();

#endif