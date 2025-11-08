// pin_receiver.h
#ifndef PIN_RECEIVER_H
#define PIN_RECEIVER_H

#include "includes.h"
#include "telemetry_manager.h"

// Pin Receiver Module
// Purpose:
// Handles the reception of signals from the pin receiver and processes the pulse data.

/**
 * @brief Initializes the pin receiver.
 * @return true if initialization is successful, false otherwise.
 */
bool initializePinReceiver();

/**
 * @brief FreeRTOS task to process pulses from the pin receiver.
 * - Groups pulses based on period (within ±12.5% tolerance).
 * - Buffers and sends events to AWS IoT via MQTT.
 */
void pinReceiverTask(void *pvParameters);

#endif