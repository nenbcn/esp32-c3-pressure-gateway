/*
 * pressure_reader.cpp
 *
 * Módulo de adquisición de presión para ESP32 + WNK80MA (I2C).
 * Lee el sensor a frecuencia configurable, valida límites y variación, y envía las muestras a la cola global FreeRTOS g_pressureQueue.
 * Todas las validaciones y parámetros están definidos en signal_parameters.h.
 *
 * Funciones principales:
 * - bool initializePressureReader(): Inicializa I2C, mutex y cola de presión.
 * - uint32_t readRawPressure(): Lee el sensor de forma thread-safe (incluye acceso I2C y mutex).
 * - bool validatePressureReading(uint32_t): Valida límites y variación entre muestras.
 * - void pressureReaderTask(void*): Tarea FreeRTOS que adquiere, valida y encola muestras periódicamente.
 *
 * Cola de destino:
 * - g_pressureQueue: Cola global FreeRTOS para PressureReading.
 */

#include "pressure_reader.h"
#include "signal_parameters.h"
#include "system_state.h"

QueueHandle_t g_pressureQueue = NULL;
SemaphoreHandle_t g_i2cMutex = NULL;

// Guarda el último valor válido para validación de variación
static uint32_t lastValidRawValue = 0;
static bool firstSample = true;

// Validation recovery
static uint8_t consecutiveInvalidCount = 0;

// I2C error recovery
static uint32_t i2cConsecutiveErrors = 0;
static const uint32_t I2C_MAX_ERRORS_BEFORE_RESET = 10; // 10 errores = 100ms a 100Hz

/**
 * @brief Reinitializes I2C bus after consecutive errors
 */
static void reinitializeI2C() {
    Log::warn("[I2C] Reinitializing I2C bus after %u consecutive errors", i2cConsecutiveErrors);
    
    // Notify system_state of I2C recovery
    notifySystemState(EVENT_I2C_ERROR_RECOVERY);
    
    Wire.end();
    delay(10); // Brief pause for complete reset
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);
    
    // Reset validation state after I2C recovery
    firstSample = true;
    consecutiveInvalidCount = 0;
    
    Log::info("[I2C] Recovery complete, validation state reset");
}

/**
 * @brief Inicializa el sistema de lectura de presión y recursos asociados.
 *
 * - Inicializa el bus I2C con los pines y frecuencia definidos.
 * - Crea el mutex para proteger el acceso I2C entre tareas.
 * - Crea la cola global para almacenar las lecturas de presión.
 * - Si falla la creación de algún recurso, lo reporta por log y retorna false.
 *
 * @return true si la inicialización fue exitosa, false si hubo algún error.
 */
bool initializePressureReader() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);
    g_i2cMutex = xSemaphoreCreateMutex();
    if (g_i2cMutex == NULL) {
        Log::error("Failed to create I2C mutex");
        return false;
    }
    g_pressureQueue = xQueueCreate(PRESSURE_QUEUE_SIZE, sizeof(PressureReading));
    if (g_pressureQueue == NULL) {
        Log::error("Failed to create pressure queue");
        return false;
    }
    Log::info("[PressureReader] Initialization successful. I2C pins: SDA=%d SCL=%d, Freq=%d Hz, Queue size=%d", I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY, PRESSURE_QUEUE_SIZE);
    return true;
}

/**
 * @brief Lee el valor RAW del sensor de presión de forma segura (thread-safe).
 *
 * - Toma el mutex de I2C para asegurar acceso exclusivo al bus.
 * - Envía el comando de lectura al sensor usando la dirección I2C.
 * - Solicita 3 bytes de datos al sensor.
 * - Si la transmisión es exitosa y se reciben los 3 bytes, los combina en un valor RAW de 24 bits.
 * - Libera el mutex tras la lectura.
 * - Si no puede tomar el mutex o falla la transmisión, retorna 0.
 *
 * @return Valor RAW de 24 bits leído del sensor, o 0 si falla el acceso al bus o la lectura.
 */
uint32_t readRawPressure() {
    uint32_t rawValue = 0;
    bool success = false;
    static bool lastReadSuccess = true;  // Track state changes
    
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) == pdTRUE) {
        Wire.beginTransmission(WNK80MA_I2C_ADDRESS);
        Wire.write(WNK80MA_READ_COMMAND);
        int endTxResult = Wire.endTransmission(false);
        
        if (endTxResult == 0) {
            uint8_t reqBytes = 3;
            uint8_t bytesRead = Wire.requestFrom((uint8_t)WNK80MA_I2C_ADDRESS, reqBytes, (bool)true);
            int avail = Wire.available();
            
            if (avail == 3) {
                rawValue = ((uint32_t)Wire.read() << 16) |
                           ((uint32_t)Wire.read() << 8) |
                           ((uint32_t)Wire.read());
                success = true;
            } else {
                // Only log when transitioning from success to error
                if (lastReadSuccess) {
                    Log::error("[I2C] Read error - Not enough bytes available: %d", avail);
                }
            }
        } else {
            if (lastReadSuccess) {
                Log::error("[I2C] endTransmission failed: %d", endTxResult);
            }
        }
        
        xSemaphoreGive(g_i2cMutex);
    } else {
        if (lastReadSuccess) {
            Log::error("[I2C] Failed to take mutex");
        }
    }
    
    // I2C error recovery logic
    if (success) {
        // Log recovery when transitioning from error to success
        if (!lastReadSuccess) {
            Log::info("[I2C] Sensor communication recovered");
        }
        i2cConsecutiveErrors = 0; // Reset counter on success
    } else {
        i2cConsecutiveErrors++;
        if (i2cConsecutiveErrors >= I2C_MAX_ERRORS_BEFORE_RESET) {
            Log::warn("[I2C] Too many consecutive errors (%d), reinitializing...", i2cConsecutiveErrors);
            reinitializeI2C();
            i2cConsecutiveErrors = 0;
        }
    }
    
    lastReadSuccess = success;
    return rawValue;
}

/**
 * @brief Valida una lectura de presión RAW con validación adaptativa.
 *
 * Criterios de validación física (no dependen del rango de instalación):
 * 1. El valor debe estar dentro de los límites del sensor (RAW_VALUE_MIN y RAW_VALUE_MAX).
 * 2. La tasa de cambio no debe exceder el límite físicamente posible (MAX_CHANGE_PER_SAMPLE).
 * 
 * Recovery automático:
 * - Si hay MAX_CONSECUTIVE_INVALID lecturas inválidas consecutivas, se resetea el baseline.
 * - Esto permite adaptarse a cambios reales en el rango de presión (cambio de instalación, etc).
 *
 * Esta estrategia funciona en cualquier rango de presión (2-7 bar) sin necesidad de calibración.
 *
 * @param rawValue Valor RAW leído del sensor.
 * @return true si la muestra es válida, false si es inválida.
 */
bool validatePressureReading(uint32_t rawValue) {
    // 1. Validación básica del sensor (límites hardware del ADC de 24-bit)
    if (rawValue <= RAW_VALUE_MIN || rawValue >= RAW_VALUE_MAX) {
        consecutiveInvalidCount++;
        if (consecutiveInvalidCount >= MAX_CONSECUTIVE_INVALID) {
            // Posible problema con el sensor o desconexión
            Log::warn("[Validation] %u consecutive out-of-range readings, resetting baseline", consecutiveInvalidCount);
            firstSample = true;
            consecutiveInvalidCount = 0;
        }
        return false;
    }
    
    // 2. Validación física: tasa de cambio máxima posible
    #if ENABLE_VARIATION_VALIDATION
        if (!firstSample) {
            uint32_t variation = (rawValue > lastValidRawValue) ? 
                                (rawValue - lastValidRawValue) : 
                                (lastValidRawValue - rawValue);
            
            // Límite físico: cambio máximo posible en 10ms (a 100Hz)
            // Esto detecta spikes/ruido pero permite cambios reales de presión
            if (variation > MAX_CHANGE_PER_SAMPLE) {
                consecutiveInvalidCount++;
                
                #ifdef DEBUG_MODE
                    Log::warn("[Validation] Spike detected: %lu (max physical: %lu)", 
                             variation, (uint32_t)MAX_CHANGE_PER_SAMPLE);
                #endif
                
                // Recovery: si persiste, probablemente es un cambio real de rango base
                if (consecutiveInvalidCount >= MAX_CONSECUTIVE_INVALID) {
                    Log::warn("[Validation] Persistent deviation after %u samples, accepting new baseline: %lu -> %lu", 
                             consecutiveInvalidCount, lastValidRawValue, rawValue);
                    lastValidRawValue = rawValue;
                    firstSample = false;
                    consecutiveInvalidCount = 0;
                    return true;
                }
                return false;
            }
        }
    #endif
    
    // Válido - resetear contador de inválidos y actualizar referencia
    consecutiveInvalidCount = 0;
    lastValidRawValue = rawValue;
    firstSample = false;
    return true;
}

/**
 * @brief Tarea principal de FreeRTOS para la adquisición periódica de presión.
 *
 * - Ejecuta un bucle infinito con periodo fijo (definido por SENSOR_SAMPLE_INTERVAL_MS).
 * - Lee el valor RAW del sensor de presión.
 * - Valida la lectura usando validatePressureReading (límites y variación).
 * - Marca la estructura con isValid según la validación.
 * - Si la lectura es inválida y está en modo DEBUG, lo reporta por log.
 * - Envía la muestra (válida o no) a la cola global g_pressureQueue.
 * - Si la cola está llena, cuenta los fallos y reporta por log al primer fallo y luego cada 100.
 * - Mantiene la temporización precisa usando vTaskDelayUntil.
 *
 * @param pvParameters No usado.
 */
void pressureReaderTask(void *pvParameters) {
    (void)pvParameters;
    PressureReading reading;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(SENSOR_SAMPLE_INTERVAL_MS); // Periodo configurable
    static uint32_t queueFailCount = 0;
    static uint32_t readingCount = 0;
    
    for (;;) {
        reading.timestamp = millis();
        reading.rawValue = readRawPressure();
        reading.isValid = validatePressureReading(reading.rawValue);
        
        #ifdef DEBUG_MODE
            readingCount++;
            // Print only every 100 samples (1 second at 100Hz) to avoid performance impact
            if (readingCount % 100 == 0) {
                Serial.printf("[Reader] Sample %lu: RAW=%lu %s\n", 
                             readingCount, reading.rawValue, reading.isValid ? "✓" : "✗");
            }
            
            if (!reading.isValid) {
                Log::warn("[PressureReader] Invalid reading: %lu", reading.rawValue);
            }
        #endif
        
        if (xQueueSend(g_pressureQueue, &reading, 0) != pdPASS) {
            queueFailCount++;
            
            // Notify system_state only on first failure to avoid spam
            if (queueFailCount == 1) {
                notifySystemState(EVENT_PRESSURE_QUEUE_FULL);
            }
            
            if (queueFailCount == 1 || (queueFailCount % 100 == 0)) {
                Log::error("[PressureReader] Pressure queue full! Lost samples: %lu", queueFailCount);
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}


