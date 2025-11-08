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

QueueHandle_t g_pressureQueue = NULL;
SemaphoreHandle_t g_i2cMutex = NULL;

// Guarda el último valor válido para validación de variación
static uint32_t lastValidRawValue = 0;
static bool firstSample = true;

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
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) == pdTRUE) {
        // Log::debug("[I2C] Mutex taken");
        Wire.beginTransmission(WNK80MA_I2C_ADDRESS);
        // Log::debug("[I2C] beginTransmission to 0x%02X", WNK80MA_I2C_ADDRESS);
        Wire.write(WNK80MA_READ_COMMAND);
        // Log::debug("[I2C] write command 0x%02X", WNK80MA_READ_COMMAND);
        int endTxResult = Wire.endTransmission(false);
        // Log::debug("[I2C] endTransmission(false) result: %d", endTxResult);
        if (endTxResult == 0) {
            uint8_t reqBytes = 3;
            // Log::debug("[I2C] requestFrom 0x%02X, %d bytes", WNK80MA_I2C_ADDRESS, reqBytes);
            uint8_t bytesRead = Wire.requestFrom((uint8_t)WNK80MA_I2C_ADDRESS, reqBytes, (bool)true);
            // Log::debug("[I2C] requestFrom returned: %d", bytesRead);
            int avail = Wire.available();
            // Log::debug("[I2C] Wire.available(): %d", avail);
            if (avail == 3) {
                rawValue = ((uint32_t)Wire.read() << 16) |
                           ((uint32_t)Wire.read() << 8) |
                           ((uint32_t)Wire.read());
                // Log::debug("[I2C] Read rawValue: %lu", rawValue);
            } else {
                Log::error("[I2C] Not enough bytes available: %d", avail);
            }
        } else {
            Log::error("[I2C] endTransmission failed: %d", endTxResult);
        }
        xSemaphoreGive(g_i2cMutex);
        // Log::debug("[I2C] Mutex released");
    } else {
        Log::error("[I2C] Failed to take mutex");
    }
    return rawValue;
}

/**
 * @brief Valida una lectura de presión RAW.
 *
 * Criterios de validación:
 * 1. El valor debe estar dentro de los límites RAW_VALUE_MIN y RAW_VALUE_MAX.
 * 2. Si no es la primera muestra, la variación absoluta respecto a la última muestra válida
 *    no debe superar MAX_SAMPLE_VARIATION (ajustada a la frecuencia de muestreo).
 *
 * Si ambas condiciones se cumplen, la muestra es válida y se actualiza el último valor válido.
 * Si alguna condición falla, la muestra se marca como inválida.
 *
 * @param rawValue Valor RAW leído del sensor.
 * @return true si la muestra es válida, false si es inválida.
 */
bool validatePressureReading(uint32_t rawValue) {
    // Validar solo rango absoluto - permitir cambios rápidos de presión legítimos
    // (aperturas/cierres de grifo, cambios de caudal, etc.)
    bool inRange = (rawValue > RAW_VALUE_MIN && rawValue < RAW_VALUE_MAX);
    
    if (inRange) {
        lastValidRawValue = rawValue;
        firstSample = false;
        return true;
    }
    
    return false;
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
        
        // Print 1 of every 10 readings with newline (every 1 second at 10Hz)
        readingCount++;
        if (readingCount % 10 == 0) {
            Serial.printf("[Reader] RAW=%lu %s (sample %lu)\n", reading.rawValue, reading.isValid ? "✓" : "✗", readingCount);
        } else {
            Serial.printf("\r[Reader] RAW=%lu %s     ", reading.rawValue, reading.isValid ? "✓" : "✗");
        }
        
#ifdef DEBUG_MODE
        if (!reading.isValid) {
            Log::warn("[PressureReader] Invalid reading: %lu", reading.rawValue);
        }
#endif
        if (xQueueSend(g_pressureQueue, &reading, 0) != pdPASS) {
            queueFailCount++;
            if (queueFailCount == 1 || (queueFailCount % 100 == 0)) {
                Log::error("[PressureReader] Pressure queue full! Lost samples: %lu", queueFailCount);
            }
        }
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}


