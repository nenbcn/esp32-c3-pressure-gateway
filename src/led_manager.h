// led_manager.h
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "includes.h" 

// LED Manager Module
// Purpose:
// Manages visual feedback through LEDs based on the current system state.

/**
 * @brief Initializes the LED manager.
 */
void initializeLedManager();

/**
 * @brief FreeRTOS task that updates LED states based on the current system state.
 * @param pvParameters Task parameters (not used).
 */
void ledTask(void *pvParameters);

/**
 * @brief Triggers a blue LED blink to indicate pressure change detection.
 * This is a non-blocking call that sets a flag for the LED task to handle.
 */
void triggerPressureChangeLed();

#endif