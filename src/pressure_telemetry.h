
#ifndef PRESSURE_TELEMETRY_H
#define PRESSURE_TELEMETRY_H

#include <Arduino.h>
#include "data_types.h"
#include "pressure_reader.h"
#include "signal_parameters.h"
#include "Log.h"

/**
 * @brief Initializes the pressure telemetry system.
 * Creates event queue and initializes signal processing state.
 * @return true if initialization successful, false otherwise.
 */
bool initializePressureTelemetry();

/**
 * @brief FreeRTOS task for pressure signal processing and event detection.
 * Reads from g_pressureQueue, applies EPA filtering, calculates derivative,
 * implements signal state machine, and sends events to g_pressureEventQueue.
 * @param pvParameters Task parameters (not used).
 */
void pressureTelemetryTask(void *pvParameters);

/**
 * @brief Calculates derivative from the sliding window buffer.
 * Uses EPA-filtered values and timestamps for robust derivative calculation.
 * @param window Pointer to the derivative window buffer
 * @return Current derivative value (pressure units per second)
 */
float calculateDerivative(const DerivativeWindow* window);

/**
 * @brief Adds a sample to the derivative window buffer.
 * @param window Pointer to the derivative window buffer  
 * @param value EPA2-filtered pressure value
 * @param timestamp Sample timestamp
 */
void addToDerivativeWindow(DerivativeWindow* window, float value, uint64_t timestamp);

/**
 * @brief Updates signal state machine based on current derivative value.
 * Implements state transitions with hysteresis for stability.
 * @param stateMachine Pointer to the state machine context
 * @param currentDerivative Current filtered derivative value
 * @param currentTime Current timestamp
 * @return true if state changed, false otherwise
 */
bool updateSignalStateMachine(SignalStateMachine* stateMachine, float currentDerivative, uint64_t currentTime);

/**
 * @brief Processes a sample during stable period.
 * Accumulates statistics and checks for period completion.
 * @param accumulator Pointer to stable period accumulator
 * @param filteredValue Current EPA2 filtered value
 * @param timestamp Current sample timestamp
 * @return true if stable period should be finalized
 */
bool processStablePeriod(StableAccumulator* accumulator, uint32_t filteredValue, uint64_t timestamp);

/**
 * @brief Processes a sample during changing period.
 * Collects detailed samples for event reconstruction.
 * @param event Pointer to current event being built
 * @param filteredValue Current EPA2 filtered value
 * @param timestamp Current sample timestamp
 * @param derivative Current derivative value
 * @return true if changing period should be finalized
 */
bool processChangingPeriod(PressureEvent* event, uint32_t filteredValue, uint64_t timestamp, float derivative);

/**
 * @brief Finalizes and sends a stable period as an event.
 * Creates event from accumulated statistics.
 * @param accumulator Pointer to completed stable accumulator
 * @param endTimestamp Final timestamp for the period
 */
void finalizeStablePeriod(const StableAccumulator* accumulator, uint64_t endTimestamp);

/**
 * @brief Finalizes and sends a changing period as an event.
 * Sends complete event with detailed samples.
 * @param event Pointer to completed event
 */
void finalizeChangingEvent(PressureEvent* event);

#endif // PRESSURE_TELEMETRY_H
