#ifndef BOARD_H
#define BOARD_H

#ifdef ESP32_C3
    // NeoPixel configuration for ESP32-C3
    #define NEOPIXEL_PIN 5    // D3 pin (GPIO05)
    #define NEOPIXEL_COUNT 1  // Single NeoPixel
    
    // Keep individual LED pins for compatibility (not used with NeoPixel)
    #define GREEN_LED_PIN 4
    #define RED_LED_PIN 10
    #define BLUE_LED_PIN 5

    #define SENSOR_PIN 21

    #define BUTTON_PIN 9
#else
    // Standard LED pins for ESP32 WROOM
    #define GREEN_LED_PIN 2
    #define RED_LED_PIN 4
    #define BLUE_LED_PIN 15

    #define SENSOR_PIN 22

    #define BUTTON_PIN 5
#endif

#endif