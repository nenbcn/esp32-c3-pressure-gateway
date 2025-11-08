// ota_manager.cpp
#include "ota_manager.h"
#include "secrets.h"
#include <Preferences.h>

// Global secure client instance
WiFiClientSecure secureClient;

void initializeOTAManager() {
    // secureClient.setCACert(AWS_CERT_CA);
    secureClient.setInsecure();
    Log::info("OTA Manager initialized successfully.");
}

void otaTask(void *pvParameters) {
    triggerOTAUpdate();

    if (getSystemState() == SYSTEM_STATE_ERROR) {
        Log::error("OTA update failed critically. Restarting device...");
        vTaskDelay(pdMS_TO_TICKS(1000));  // Allow log messages to be sent.
        ESP.restart();
    }

    // If no update was available (non-critical case), log and return to normal operation.// If no update was available (non-critical case), simply log and clean up.
    Log::info("OTA update task completed (no update available). Returning to normal operation.");
    setSystemState(SYSTEM_STATE_CONNECTED_MQTT);
    setOtaTaskHandle(NULL);
    vTaskDelete(NULL);  // Cleanly delete this task.
}

void triggerOTAUpdate() {
    Preferences preferences;
    preferences.begin("ota", true); 
    String firmwareUrl = preferences.getString("url", "");
    preferences.end();
    if (firmwareUrl.length() == 0) {
        Log::error("No firmware URL found in EEPROM.");
        setSystemState(SYSTEM_STATE_ERROR);
        return;
    }
    Log::info("Retrieved firmwareUrl length: %d", firmwareUrl.length());
    Log::info("Starting OTA update from URL: %s", firmwareUrl.c_str());
    vTaskDelay(pdMS_TO_TICKS(2000));
    t_httpUpdate_return ret = httpUpdate.update(secureClient, firmwareUrl);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Log::error("OTA update failed. Error (%d): %s", 
                       httpUpdate.getLastError(),
                       httpUpdate.getLastErrorString().c_str());
            setSystemState(SYSTEM_STATE_ERROR);
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Log::info("OTA update: No updates available.");
            setSystemState(SYSTEM_STATE_CONNECTED_MQTT);
            break;

        case HTTP_UPDATE_OK:
            Log::info("OTA update successful but device did not reboot automatically. Restarting manually...");
            ESP.restart();
            break;

        default:
            Log::error("OTA update returned unknown status: %d", ret);
            setSystemState(SYSTEM_STATE_ERROR);
            break;
    }
}