

#ifndef PRESSURE_READER_H
#define PRESSURE_READER_H




#include <Arduino.h>

typedef struct {
	unsigned long timestamp;
	uint32_t rawValue;
	bool isValid;
} PressureReading;
#include "Log.h"
#include "board.h"
#include "signal_parameters.h"
#include <Wire.h>


extern QueueHandle_t g_pressureQueue;
extern SemaphoreHandle_t g_i2cMutex;


// Pressure Reader Module - Replaces LoRa Receiver
// Purpose: Read WNK80MA pressure sensor via I2C at 100Hz

// Hardware configuration from hardware_specs.md  
#define WNK80MA_I2C_ADDRESS 0x6D
#define WNK80MA_READ_COMMAND 0x06
#define I2C_FREQUENCY 100000
#define I2C_TIMEOUT_MS 10



// Queue size definition - sized for adequate buffering
// At 100Hz, we need buffer for processing delays + safety margin
// Processing happens every 1000ms, so we need at least 200 samples (2 seconds)
// Adding safety margin for processing time: 300 samples (3 seconds buffer)
#define PRESSURE_QUEUE_SIZE 300

/**
 * @brief Initializes I2C communication and pressure reading system.
 * @return true if initialization is successful, false otherwise.
 */
bool initializePressureReader();

/**
 * @brief Reads RAW pressure value from WNK80MA sensor via I2C.
 * @return RAW 24-bit pressure value, 0 if read fails.
 */
uint32_t readRawPressure();

/**
 * @brief Validates if RAW pressure value is within acceptable range.
 * @param rawValue The RAW value to validate.
 * @return true if value is valid, false otherwise.
 */
bool validatePressureReading(uint32_t rawValue);

/**
 * @brief FreeRTOS task to continuously read pressure sensor at 100Hz.
 * @param pvParameters Task parameters (not used).
 */
void pressureReaderTask(void *pvParameters);


#endif // PRESSURE_READER_H
