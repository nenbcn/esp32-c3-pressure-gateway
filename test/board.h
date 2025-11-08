#ifndef BOARD_H
#define BOARD_H

// Board identification
#define BOARD_NAME "ESP32-C3-XIAO"
#define BOARD_VERSION "1.0"

// --- Pinout Configuration for ESP32-C3 ---

// I2C pins for WNK80MA pressure sensor
#define I2C_SDA_PIN 6 // D4
#define I2C_SCL_PIN 7

// NeoPixel for status LED
#define NEOPIXEL_PIN 5  // D3 pin (GPIO5)
#define NEOPIXEL_COUNT 1

// Button
#define BUTTON_PIN 9 // D9 GPIO9 

// --- Common Definitions ---


#endif //
