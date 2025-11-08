/// wifi_connect.cpp
#include "wifi_connect.h"
#include "system_state.h"
#include "ESP32Ping.h"

#define CONNECTION_CHECK_INTERVAL_MS 60 * 1000
#define CONNECTION_TIMEOUT_MS 60UL * 1000UL

// Remote address to ping to check connectivity.
// In this case we use IANA example.com
const IPAddress remote(192, 0, 43, 10);

// Internal Variables
static SemaphoreHandle_t wifiMutex = NULL;

// Initialize WiFi Connection
bool initializeWiFiConnection() {
    WiFi.mode(WIFI_STA);
    //WiFi.setSleep(true); // Optional power saving
    wifiMutex = xSemaphoreCreateMutex();
    if (wifiMutex == NULL) {
        Log::error("Failed to create WiFi mutex.");
        return false;
    }
    Log::info("WiFi hardware initialized in station mode.");
    return true;
}

bool hasAssignedIp() {
    return WiFi.localIP() != 0u;
}

bool canReachRemote() {
    return Ping.ping(remote);
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED && hasAssignedIp() && canReachRemote();
}

// WiFi Connect Task
void wifiConnectTask(void *pvParameters) {
    while (true) {
        // Check if already connected
        if (isConnected()) {
            notifySystemState(EVENT_WIFI_CONNECTED);
            vTaskDelay(pdMS_TO_TICKS(CONNECTION_CHECK_INTERVAL_MS)); // Pause before checking again
            continue;
        }

        Log::warn("Wi-Fi disconnected. Attempting to reconnect...");

        String ssid, password;

        if (loadCredentials(ssid, password)) {
            if (ssid.isEmpty() || password.isEmpty()) {
                Log::warn("No Wi-Fi credentials found in EEPROM.");
                notifySystemState(EVENT_NO_PARAMETERS_EEPROM);
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            Log::info("Attempting to connect to SSID: %s", ssid.c_str());
            WiFi.disconnect(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            WiFi.begin(ssid.c_str(), password.c_str());

            unsigned long startTime = millis();

            // Wait for connection
            while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < CONNECTION_TIMEOUT_MS) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                Log::debug("Connecting...");
            }

            // Check the result
            if (WiFi.status() == WL_CONNECTED) {
                Log::info("Connected to Wi-Fi! IP Address: %s", WiFi.localIP().toString().c_str());
                notifySystemState(EVENT_WIFI_CONNECTED);
            } else {
                Log::error("Failed to connect to Wi-Fi.");
                notifySystemState(EVENT_WIFI_FAIL_CONNECT);
            }
        } else {
            Log::warn("No credentials found in EEPROM.");
            notifySystemState(EVENT_NO_PARAMETERS_EEPROM); // TODO: Se usa la misma que en ssid y password empty, comprobar que se quiera hacer así
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Pause before next attempt
    }
}