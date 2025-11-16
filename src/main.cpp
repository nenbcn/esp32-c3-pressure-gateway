// main.cpp
#include "includes.h"
#include "Log.h"
#include "system_state.h"

void setup() {
    // Initialize serial communication at 115200 baud
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n===========================================");
    Serial.println("ESP32-C3 Pressure Gateway - Production Mode");
    Serial.println("100Hz sampling with derivative-based event detection");
    Serial.println("==========================================");

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
