// button_manager.cpp
#include "button_manager.h"
#include "board.h"

// Constants
const unsigned long LONG_PRESS_TIME = 5000;  // Time to consider a long press (ms)
const unsigned long DEBOUNCE_TIME = 50;      // Minimum time to avoid bounces (ms)

// Global Variables
static unsigned long buttonPressStart = 0; // Timestamp for long press detection
static unsigned long lastButtonCheck = 0;  // Timestamp for debounce

// Interrupt Service Routine (ISR) for button events
void IRAM_ATTR buttonISR() {
    // This ISR is kept for future use but is currently not notifying any event
    // to avoid conflicts with the long-press detection logic in buttonTask.
    // The button state is polled directly in buttonTask.
}


// ========================================================
// Initialize Button Manager
void initializeButtonManager() {
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Configurar el botón como entrada con pull-up
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE); // Asociar la ISR al pin del botón
    Log::info("Button Manager initialized. Waiting for button events.");
}

// Button Task
void buttonTask(void *pvParameters) {
    unsigned long pressStartTime = 0;
    bool longPressNotified = false;

    while (true) {
        unsigned long currentMillis = millis();
        int buttonState = digitalRead(BUTTON_PIN);

        if (buttonState == LOW) { // Button is pressed
            if (pressStartTime == 0) {
                // Button was just pressed
                pressStartTime = currentMillis;
                longPressNotified = false;
                Log::debug("Button press started at %lu", pressStartTime);
            }

            // Check for long press
            if (!longPressNotified && (currentMillis - pressStartTime >= LONG_PRESS_TIME)) {
                Log::warn("Long press detected. Notifying system.");
                notifySystemState(EVENT_LONG_PRESS_BUTTON);
                longPressNotified = true; // Ensure notification is sent only once
            }
        } else { // Button is not pressed
            if (pressStartTime != 0) {
                // Button was just released
                Log::debug("Button released.");
                pressStartTime = 0; // Reset timer
                longPressNotified = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Poll every 50ms
    }
}