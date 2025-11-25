#ifndef SIGNAL_PARAMETERS_H
#define SIGNAL_PARAMETERS_H

// ===================================================================
// SIGNAL PROCESSING ALGORITHM PARAMETERS FOR WNK80MA PRESSURE SENSOR
// ===================================================================


// --- Sampling Configuration ---
// SENSOR_SAMPLE_RATE_HZ: Main sampling frequency for the pressure sensor (Hz).
// Changing this value affects:
//   - SENSOR_SAMPLE_INTERVAL_MS (interval between samples)
//   - DERIVATIVE_THRESHOLD (umbral de derivada por muestra)
//   - MAX_SAMPLE_VARIATION (variación máxima permitida entre muestras)
//   - El tamaño efectivo de los batches y la resolución temporal de eventos
//   - El consumo de CPU y memoria
// Ajusta los demás parámetros dependientes si cambias este valor.
#define SENSOR_SAMPLE_RATE_HZ 100
#define SENSOR_SAMPLE_INTERVAL_MS (1000 / SENSOR_SAMPLE_RATE_HZ)

// --- EPA Filter Parameters ---
// EPA_ALPHA_PRIMARY and EPA_ALPHA_SECONDARY:
// Coefficients for the two cascaded Exponential Moving Average (EMA/EPA) filters.
// A higher value (closer to 1) makes the filter more reactive (less smoothing, more sensitive to noise).
// A lower value (closer to 0) increases smoothing but can delay detection of fast changes.
// Double EPA filtering is used (instead of a single slower filter) to achieve better attenuation of high-frequency noise
// without losing responsiveness to real changes, and to avoid the excessive lag that a single very slow filter would introduce.
#define EPA_ALPHA_PRIMARY 0.1f      // Primary filter coefficient (más agresivo)
#define EPA_ALPHA_SECONDARY 0.05f   // Secondary filter coefficient (más suave)

// --- DERIVATIVE DETECTION PARAMETERS (Production Algorithm) ---
#define DERIVATIVE_WINDOW_SIZE 50                    // Window size for derivative calculation (0.5s at 100Hz)
#define DERIVATIVE_THRESHOLD_PER_SEC 120000.0f       // Derivative threshold per second (raw units/s)
#define DERIVATIVE_THRESHOLD (DERIVATIVE_THRESHOLD_PER_SEC / SENSOR_SAMPLE_RATE_HZ)
#define DERIVATIVE_FILTER_ALPHA 0.1f                 // Alpha for derivative smoothing filter
#define MIN_EVENT_DURATION_MS 50                     // Minimum event duration
#define EVENT_HYSTERESIS_FACTOR 0.8f                 // Hysteresis factor (80% to exit changing state)

// --- ADAPTIVE INTERVAL PARAMETERS ---
// Signal state machine for optimized data transmission with precise state transitions
#define MIN_STABLE_DURATION_MS 2000              // Minimum time in stable state before transitioning (2s)
#define MAX_STABLE_EVENT_DURATION_MS 60000       // Maximum duration for STABLE events (1 minute)
#define MAX_CHANGING_EVENT_DURATION_MS 3000      // Maximum duration for CHANGING events (3 seconds)

// --- EVENT SAMPLING PARAMETERS ---
// Maximum samples stored per event (for detailed change intervals)
#define MAX_SAMPLES_PER_EVENT 100                // Max detailed samples per changing event (1s @ 100Hz) - reduced to save memory

// --- DATA VALIDATION PARAMETERS ---
// Raw value limits for sensor validation
#define RAW_VALUE_MIN 10000UL                   // Minimum valid raw sensor value
#define RAW_VALUE_MAX 16000000UL                // Maximum valid raw sensor value

// Sample-to-sample variation limits (physics-based, not installation-specific)
#define ENABLE_VARIATION_VALIDATION true        // Enable variation checking
#define MAX_PRESSURE_CHANGE_PER_SECOND 500000.0f  // Maximum physically possible change (raw units/s)
#define MAX_CHANGE_PER_SAMPLE (MAX_PRESSURE_CHANGE_PER_SECOND / SENSOR_SAMPLE_RATE_HZ)  // Per 10ms at 100Hz

// Recovery parameters
#define MAX_CONSECUTIVE_INVALID 20              // Reset baseline after 20 invalid samples (200ms at 100Hz)



// =========================
// QUEUE AND TASK PARAMETERS
// =========================

// Pressure Data Queues
// These parameters control the buffering of raw sensor samples and detected events.
// You can increase the sizes to improve tolerance to processing/network delays, but this will use more RAM.
// Lower values reduce memory usage but increase the risk of data loss if the system is overloaded.
#define PRESSURE_QUEUE_SIZE 300             // RAW buffer size (samples)
#define PRESSURE_EVENT_QUEUE_SIZE 10        // Event queue size
#define MQTT_QUEUE_SIZE 10                  // Queue for MQTT messages

// Task Stack Sizes and Priorities
// These parameters define the FreeRTOS stack size (in bytes) and priority for each main task.
// If you increase the size of queues or batches, you may also need to increase the stack size of tasks that process them,
// since larger buffers or local copies require more stack memory. As a rule of thumb:
//   Required stack ≈ base stack + (record size × max batch/queue size) + safety margin
// Monitor for stack overflows if you change these values.
// Lowering stack size too much may cause crashes; changing priorities can affect system responsiveness.
#define PRESSURE_READER_STACK_SIZE 3072     // Reader task stack (bytes)
#define PRESSURE_READER_PRIORITY 5          // Reader task priority
#define PRESSURE_TELEMETRY_STACK_SIZE 8192  // Telemetry task stack (bytes) - same as mica-gateway
#define PRESSURE_TELEMETRY_PRIORITY 3       // Telemetry task priority

#define MESSAGE_FORMATTER_STACK_SIZE 10240  // Message formatter task stack (bytes) - for JSON processing with ArduinoJson (needs extra for initialization)
#define MESSAGE_FORMATTER_PRIORITY 2        // Message formatter priority

// --- Processing Intervals ---
#define TELEMETRY_PROCESS_INTERVAL_MS 100    // Telemetry: Process every 100ms (10 samples at 100Hz)
#define FORMATTER_PROCESS_INTERVAL_MS 100    // Formatter: Check for events every 100ms

// --- Message Formatter Configuration ---
// These parameters control batching and timeout for MQTT message generation
#define MAX_EVENTS_PER_MESSAGE 8             // Max events per MQTT message
#define FORMATTER_SEND_TIMEOUT_MS 2000       // Force send every 2 seconds

#endif // SIGNAL_PARAMETERS_H
