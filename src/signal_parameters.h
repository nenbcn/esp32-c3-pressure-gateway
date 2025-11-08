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
#define SENSOR_SAMPLE_RATE_HZ 10
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

// --- Change Detection for Adaptive Intervals ---
// PRESSURE_CHANGE_THRESHOLD: Absolute change threshold to close current interval and start a new one
// PRESSURE_CHANGE_PERCENT: Relative change threshold (percentage of current value)
// A new interval is created when EITHER threshold is exceeded
// Both thresholds calibrated for ~4 bar pressure (~3,450,000 raw units)
#define PRESSURE_CHANGE_THRESHOLD 35000      // Absolute change threshold (raw units) - ~1% at 4 bar
#define PRESSURE_CHANGE_PERCENT 1.0f         // Relative change threshold (1.0% = significant change)

// --- COMMENTED OUT: Complex event detection (not used in simplified version) ---
// #define DERIVATIVE_WINDOW_SIZE 50        
// #define DERIVATIVE_THRESHOLD_PER_SEC 120000.0f 
// #define DERIVATIVE_THRESHOLD (DERIVATIVE_THRESHOLD_PER_SEC / SENSOR_SAMPLE_RATE_HZ)
// #define DERIVATIVE_FILTER_ALPHA 0.1f    
// #define MIN_EVENT_DURATION_MS 50        
// #define PRE_EVENT_PERIOD_MS 400         
// #define POST_EVENT_PERIOD_MS 0          
// #define EVENT_HYSTERESIS_FACTOR 0.8f



// =========================
// QUEUE AND TASK PARAMETERS
// =========================

#// Pressure Data Queues
#// These parameters control the buffering of raw sensor samples and detected events.
#// You can increase the sizes to improve tolerance to processing/network delays, but this will use more RAM.
#// Lower values reduce memory usage but increase the risk of data loss if the system is overloaded.
#define PRESSURE_QUEUE_SIZE 300             // RAW buffer size (samples)
#define PRESSURE_EVENT_QUEUE_SIZE 10        // Event queue size

#// Task Stack Sizes and Priorities
#// These parameters define the FreeRTOS stack size (in bytes) and priority for each main task.
#// If you increase the size of queues or batches, you may also need to increase the stack size of tasks that process them,
#// since larger buffers or local copies require more stack memory. As a rule of thumb:
#//   Required stack ≈ base stack + (record size × max batch/queue size) + safety margin
#// Monitor for stack overflows if you change these values.
#// Lowering stack size too much may cause crashes; changing priorities can affect system responsiveness.
#define PRESSURE_READER_STACK_SIZE 4096     // Reader task stack (bytes)
#define PRESSURE_READER_PRIORITY 5          // Reader task priority
#define PRESSURE_TELEMETRY_STACK_SIZE 8192  // Telemetry task stack (bytes) - doubled for JSON serialization safety
#define PRESSURE_TELEMETRY_PRIORITY 3       // Telemetry task priority

// --- Telemetry Configuration for Real-Time Monitoring ---
// MAX_INTERVALS_PER_MESSAGE: Maximum number of intervals to accumulate before sending
// TELEMETRY_PROCESS_INTERVAL_MS: How often to process pressure queue (300ms for responsiveness)
// TELEMETRY_SEND_TIMEOUT_MS: Force send even if buffer not full (1s for real-time graphing)
#define MAX_INTERVALS_PER_MESSAGE 5          // Max intervals per MQTT message (reduced to limit JSON size)
#define TELEMETRY_PROCESS_INTERVAL_MS 300    // Process every 300ms
#define TELEMETRY_SEND_TIMEOUT_MS 1000       // Force send every 1 second
#define MQTT_QUEUE_SIZE 10                   // Queue for MQTT messages

// --- COMMENTED OUT: Old batch parameters (not used in simplified version) ---
// #define TELEMETRY_BATCH_SIZE 100             
// #define PRESSURE_BATCH_QUEUE_SIZE 10        
// #define BATCH_FLUSH_ON_EVENT_END true       
// #define MQTT_SEND_INTERVAL_MS 60000           
// #define STATUS_REPORT_INTERVAL_MS 60000

// --- Data Validation ---
#define RAW_VALUE_MIN 10000UL          // Minimum valid RAW value
#define RAW_VALUE_MAX 16000000UL        // Maximum valid RAW value
#define MAX_SAMPLE_VARIATION_100HZ 300000UL    // Máximo permitido para 100Hz
#define MAX_SAMPLE_VARIATION ( (MAX_SAMPLE_VARIATION_100HZ) * (SENSOR_SAMPLE_RATE_HZ/100.0f) )

#endif // SIGNAL_PARAMETERS_H
