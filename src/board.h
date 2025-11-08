#ifndef BOARD_H
#define BOARD_H

#ifdef ESP32_C3
    // Board identification
    #define BOARD_NAME "ESP32-C3-XIAO"
    #define BOARD_VERSION "1.0"

    // I2C pins for WNK80MA pressure sensor
    #define I2C_SDA_PIN 6 // D4
    #define I2C_SCL_PIN 7 // D5

    // NeoPixel for status LED
    #define NEOPIXEL_PIN 5    // D3 pin (GPIO5)
    #define NEOPIXEL_COUNT 1  // Single NeoPixel

    // Button
    #define BUTTON_PIN 9 // D9 GPIO9

    // Legacy LED pins (not used with NeoPixel, kept for compatibility)
    #define GREEN_LED_PIN 4
    #define RED_LED_PIN 10
    #define BLUE_LED_PIN 5
#else
    // Standard LED pins for ESP32 WROOM
    #define GREEN_LED_PIN 2
    #define RED_LED_PIN 4
    #define BLUE_LED_PIN 15

    #define BUTTON_PIN 5
#endif

#endif // BOARD_H
