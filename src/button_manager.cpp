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
    static unsigned long lastDebounceTime = 0;
    unsigned long currentMicros = micros(); // Usamos micros(), que sí funciona en ISR
     Log::debug("interrupcion boton detectada");// eliminar no es ok en una interrupcon
    if (currentMicros - lastDebounceTime < DEBOUNCE_TIME * 1000) {
        return;  // Ignorar eventos si están dentro del tiempo de debounce
    }
    lastDebounceTime = currentMicros;

    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW) {
        notifySystemState(EVENT_BUTTON_PRESSED);
    } else {
        notifySystemState(EVENT_BUTTON_RELEASED);
    }
}


// ========================================================
// Initialize Button Manager
void initializeButtonManager() {
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Configurar el botón como entrada con pull-down
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE); // Asociar la ISR al pin del botón para eventos de cambio
    Log::info("Button Manager initialized. Waiting for button events.");
}

// Button Task
void buttonTask(void *pvParameters) {
    bool longPressSent = false; // Avoid multiple long press notifications

    while (true) {
        unsigned long currentMillis = millis();

        // Read button state
        int buttonState = digitalRead(BUTTON_PIN);

        // Verifica si el botón está presionado
        if (buttonState == LOW) {
            if (buttonPressStart == 0) {
                buttonPressStart = currentMillis; // Save press start time
                longPressSent = false; // Reset long press detection
                Log::debug("Button press detected. Waiting to verify long press...");
            }

            //  Check if long press time has passed and hasn't been sent yet
            if (!longPressSent && (currentMillis - buttonPressStart > LONG_PRESS_TIME)) {
                notifySystemState(EVENT_LONG_PRESS_BUTTON);
                longPressSent = true; // Avoid repeated sends
            }
        } else { // Button released
            if (buttonPressStart != 0) {
                Log::debug("Button released before long press threshold.");
            }
            buttonPressStart = 0; // Reset press time
            longPressSent = false; // Reset for next press
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to avoid excessive CPU usage
    }
}