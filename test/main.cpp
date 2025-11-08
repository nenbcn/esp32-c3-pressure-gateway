#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lectura_ok.h"

void setup() {
  Serial.begin(115200);
  init_lectura_ok();
  xTaskCreatePinnedToCore(
    tarea_lectura_ok,
    "LecturaI2C",
    4096,
    nullptr,
    1,
    nullptr,
    0
  );
}

void loop() {
  // No hacer nada, todo va en la tarea
}