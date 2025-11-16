// led_manager.cpp
#include "board.h"
#include "led_manager.h"

// NeoPixel instance
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
    strip.setPixelColor(0, strip.Color(red, green, blue));
    strip.show();
}

void initializeLedManager() {
    // Initialize NeoPixel
    strip.begin();
    strip.setBrightness(50); // Set brightness to 50% (0-255)
    strip.show(); // Initialize all pixels to 'off'
    
    // Test sequence for NeoPixel
    setNeoPixelColor(255, 0, 0); // Red
    vTaskDelay(pdMS_TO_TICKS(500));
    setNeoPixelColor(0, 255, 0); // Green
    vTaskDelay(pdMS_TO_TICKS(500));
    setNeoPixelColor(0, 0, 255); // Blue
    vTaskDelay(pdMS_TO_TICKS(500));
    setNeoPixelColor(0, 0, 0); // Off
    
    Log::info("LED Manager initialized with NeoPixel.");
}

void ledTask(void *pvParameters) {
    Log::info("LED Task started.");

    SystemState currentState = getSystemState();
    SystemState previousState = SYSTEM_STATE_ERROR;
    bool ledState = false; // For blinking effects

    while (true) {
        switch (currentState) {
            case SYSTEM_STATE_CONNECTING:
                // Red LED blinks slowly to indicate a connecting attempt
                if (previousState != currentState) {
                    Log::debug("LED: Connecting (Red LED blinking slowly)");
                }
                setNeoPixelColor(ledState ? 255 : 0, 0, 0); // Red blinking
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case SYSTEM_STATE_CONNECTED_WIFI:
                // Green LED blinks slowly to show WiFi connection
                if (previousState != currentState) {
                    Log::debug("LED: Connected to WiFi (Green LED blinking slowly)");
                }
                setNeoPixelColor(0, ledState ? 255 : 0, 0); // Green blinking
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_CONNECTED_MQTT:
                // Green LED stays on to indicate full connectivity (WiFi + MQTT)
                if (previousState != currentState) {
                    Log::debug("LED: Connected to MQTT (Green LED ON)");
                }
                setNeoPixelColor(0, 255, 0); // Green solid
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_ERROR:
                // Red LED stays on to signal an error condition
                if (previousState != currentState) {
                    Log::warn("LED: System Error (Red LED ON)");
                }
                setNeoPixelColor(255, 0, 0); // Red solid
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_CONFIG_MODE:
                // Green LED blinks quickly for configuration mode
                if (previousState != currentState) {
                    Log::debug("LED: Configuration Mode (Green LED blinking fast)");
                }
                setNeoPixelColor(0, ledState ? 255 : 0, 0); // Green fast blinking
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(200));
                break;

            default:
                // Turn off LEDs for undefined states
                if (previousState != currentState) {
                    Log::warn("LED: Unknown State (All LEDs OFF)");
                }
                setNeoPixelColor(0, 0, 0); // Off
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
        previousState = currentState;
        currentState = getSystemState();
    }
}