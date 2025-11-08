#ifndef LECTURA_OK_H
#define LECTURA_OK_H

#include <Arduino.h>

// Inicializa el sensor (I2C, pines, etc)
void init_lectura_ok();

// Función de tarea FreeRTOS para lectura periódica
void tarea_lectura_ok(void *pvParameters);

#endif // LECTURA_OK_H