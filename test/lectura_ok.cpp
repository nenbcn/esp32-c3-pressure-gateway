/*
  === Referencia de Lectura I2C para Sensor WNK80MA ===

  Protocolo de lectura estándar:
  -----------------------------
  1. Inicialización:
    - Wire.begin(SDA, SCL)
    - Wire.setClock(400000)   // Solo una vez tras begin()

  2. Lectura de presión (cada iteración de la tarea):
    a) Wire.beginTransmission(0x6D)
    b) Wire.write(0x06)                // Comando de lectura
    c) Wire.endTransmission(false)     // Repeated start
    d) Wire.requestFrom(0x6D, 3)      // Leer 3 bytes (24 bits)
    e) Reconstruir valor raw:
      raw = (b0 << 16) | (b1 << 8) | b2; aplicar signo si es necesario

  3. Frecuencia de muestreo:
    - Se garantiza exactitud usando vTaskDelayUntil (100 Hz = 10 ms, ajustable)
    - Cambia el valor de 'periodo' para modificar la frecuencia

  4. Parámetros observados en pruebas reales:
    - Dirección I2C: 0x6D
    - Frecuencia de muestreo probada: 500 Hz (2 ms) y 100 Hz (10 ms)
    - Valor mínimo observado: 1,967,672
    - Valor máximo observado: 4,082,860
    - Máximo cambio absoluto entre lecturas consecutivas: 1,019,964
    - Robustez: miles de lecturas sin errores ni bloqueos

  5. Uso típico:
    - Dar de alta esta función como tarea con xTaskCreatePinnedToCore en setup().
    - El valor raw refleja la presión detectada por el sensor en cada instante.
*/

#include "lectura_ok.h"
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "board.h"

#define SENSOR_ADDR 0x6D

void init_lectura_ok() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000); // Se configura una sola vez tras begin()
}


void tarea_lectura_ok(void *pvParameters) {
  (void)pvParameters;
  const TickType_t periodo = pdMS_TO_TICKS(10); // 100 Hz (ajusta aquí para otras frecuencias)
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    // Lectura I2C del sensor WNK80MA
    int32_t raw;
    // Wire.setClock(400000); // No es necesario aquí, ya se configuró en la inicialización
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x06);
    int txResult = Wire.endTransmission(false);
    if (txResult != 0) {
      Serial.println("[I2C][ERROR] endTransmission");
    } else {
      uint8_t bytes = Wire.requestFrom(SENSOR_ADDR, 3u);
      if (bytes != 3) {
        Serial.println("[I2C][ERROR] No se pudo leer el sensor");
      } else {
        uint8_t b0 = Wire.read();
        uint8_t b1 = Wire.read();
        uint8_t b2 = Wire.read();
        raw = ((int32_t)b0 << 16) | ((int32_t)b1 << 8) | b2;
        if (raw & 0x800000) raw |= 0xFF000000;
        Serial.print(millis());
        Serial.print(", ");
        Serial.println(raw);
      }
    }
    vTaskDelayUntil(&xLastWakeTime, periodo); // Intervalo exacto
  }
}