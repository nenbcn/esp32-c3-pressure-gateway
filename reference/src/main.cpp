// main.cpp
#include "includes.h"
#include "Log.h"
#include "system_state.h"

void setup() {
    // Initialize serial communication at 115.200 baud
    Serial.begin(115200);

    // Initialize system state
    if (!initializeSystemState()) {
        Log::error("Failed to initialize the system. Restarting...");
        ESP.restart(); // Restart the ESP32 in case of critical failure
    }
}

void loop() {
    // Empty loop
}
