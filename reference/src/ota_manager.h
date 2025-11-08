// ota_manager.h

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "includes.h"
#include "system_state.h"

/**
 * @brief Initializes the OTA Manager.
 *
 * This function sets up the secure client for HTTPS OTA updates using certificate
 * fingerprint verification.
 */
void initializeOTAManager();

/**
 * @brief
 */
void otaTask(void *pvParameters);

/**
 * @brief Triggers the OTA update process.
 *
 * This function performs an HTTPS-based OTA update using the HTTPUpdate library.
 * It handles errors robustly and logs the outcome. On success, the device will reboot.
 */
void triggerOTAUpdate();

#endif