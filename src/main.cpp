// main.cpp
#include "includes.h"
#include "Log.h"
#include "system_state.h"
#include <esp_ota_ops.h>

void setup() {
    // Initialize serial communication at 115200 baud
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n===========================================");
    Serial.println("ESP32-C3 Pressure Gateway - Production Mode");
    Serial.println("100Hz sampling with derivative-based event detection");
    Serial.println("==========================================");

    // Mark OTA as valid to prevent rollback
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("[OTA] Firmware pending verification - marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    // Initialize system state (this creates all tasks and initializes all modules)
    if (!initializeSystemState()) {
        Log::error("Failed to initialize the system. Restarting...");
        ESP.restart(); // Restart the ESP32 in case of critical failure
    }
}

void loop() {
    // Empty loop - all work is done by FreeRTOS tasks
    vTaskDelay(portMAX_DELAY);
}
