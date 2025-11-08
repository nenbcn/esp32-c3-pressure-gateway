// led_manager.cpp
#include "board.h"
#include "led_manager.h"

#ifdef ESP32_C3
// NeoPixel instance for ESP32-C3
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
    strip.setPixelColor(0, strip.Color(red, green, blue));
    strip.show();
}
#endif

void initializeLedManager() {
#ifdef ESP32_C3
    // Initialize NeoPixel for ESP32-C3
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
    
    Log::info("LED Manager initialized with NeoPixel (ESP32-C3).");
#else
    // Initialize individual LEDs for ESP32 WROOM
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);

    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);

    Log::info("LED Manager initialized. LEDs turned ON for verification.");
    vTaskDelay(pdMS_TO_TICKS(2000));

    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BLUE_LED_PIN, HIGH);
#endif
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
#ifdef ESP32_C3
                setNeoPixelColor(ledState ? 255 : 0, 0, 0); // Red blinking
#else
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(RED_LED_PIN, ledState ? LOW : HIGH);
#endif
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case SYSTEM_STATE_CONNECTED_WIFI:
                // Green LED blinks slowly to show WiFi connection
                if (previousState != currentState) {
                    Log::debug("LED: Connected to WiFi (Green LED blinking slowly)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(0, ledState ? 255 : 0, 0); // Green blinking
#else
                digitalWrite(GREEN_LED_PIN, ledState ? LOW : HIGH);
                digitalWrite(RED_LED_PIN, HIGH);
#endif
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_CONNECTED_MQTT:
                // Green LED stays on to indicate full connectivity (WiFi + MQTT)
                if (previousState != currentState) {
                    Log::debug("LED: Connected to MQTT (Green LED ON)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(0, 255, 0); // Green solid
#else
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(RED_LED_PIN, HIGH);
#endif
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_ERROR:
                // Red LED stays on to signal an error condition
                if (previousState != currentState) {
                    Log::warn("LED: System Error (Red LED ON)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(255, 0, 0); // Red solid
#else
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(RED_LED_PIN, LOW);
#endif
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case SYSTEM_STATE_CONFIG_MODE:
                // Green LED blinks quickly for configuration mode
                if (previousState != currentState) {
                    Log::debug("LED: Configuration Mode (Green LED blinking fast)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(0, ledState ? 255 : 0, 0); // Green fast blinking
#else
                digitalWrite(GREEN_LED_PIN, ledState ? LOW : HIGH);
                digitalWrite(RED_LED_PIN, HIGH);
#endif
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(200));
                break;

            case SYSTEM_STATE_WAITING_BUTTON_RELEASE:
                // Red LED blinks quickly while waiting for button release
                if (previousState != currentState) {
                    Log::debug("LED: Waiting for button release (Red LED blinking fast)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(ledState ? 255 : 0, 0, 0); // Red fast blinking
#else
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(RED_LED_PIN, ledState ? LOW : HIGH);
#endif
                ledState = !ledState;
                vTaskDelay(pdMS_TO_TICKS(200));
                break;

            default:
                // Turn off LEDs for undefined states
                if (previousState != currentState) {
                    Log::warn("LED: Unknown State (All LEDs OFF)");
                }
#ifdef ESP32_C3
                setNeoPixelColor(0, 0, 0); // Off
#else
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(RED_LED_PIN, HIGH);
#endif
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
        previousState = currentState;
        currentState = getSystemState();
    }
}