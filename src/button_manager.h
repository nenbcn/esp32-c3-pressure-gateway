// button_manager.h
#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "includes.h"
#include "system_state.h"

// Button Manager Module
// Purpose:
// Detects button presses and provides logic for handling long presses to trigger configuration mode.

/**
 * @brief Initializes the button manager, configures the button pin, and attaches the interrupt.
 */
void initializeButtonManager();

/**
 * @brief FreeRTOS task to manage button presses, handling short and long presses.
 * @param pvParameters Task parameters (not used).
 */
void buttonTask(void *pvParameters);

#endif