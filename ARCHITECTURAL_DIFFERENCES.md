# Diferencias Arquitectónicas con mica-gateway

Este documento registra las diferencias **intencionadas** entre `esp32-c3-pressure-gateway` y `mica-gateway`, así como las que deben mantenerse alineadas.

**Fecha de análisis:** 25 Noviembre 2025  
**Referencia:** mica-gateway (ESP32 standard)

---

## ✅ Diferencias Intencionadas (Válidas)

Estas diferencias son **necesarias** debido a limitaciones de hardware o cambios en el dominio del problema:

### 1. Sensor de Presión vs LoRa/Pulsos
- **mica-gateway:** Usa `pin_receiver` para contar pulsos
- **pressure-gateway:** Usa `pressure_reader` + `pressure_telemetry` para adquisición a 100Hz

### 2. Optimización de Memoria (ESP32-C3)
- **MAX_SAMPLES_PER_EVENT:** 100 (vs 300 en mica-gateway)
  - **Razón:** ESP32-C3 tiene 320KB RAM vs 520KB del ESP32 estándar
  - **Impacto:** 33KB de memoria liberada, eventos limitados a 1s en lugar de 3s

### 3. AsyncWebServer como Puntero
- **mica-gateway:** `AsyncWebServer server(80);` (variable global)
- **pressure-gateway:** `AsyncWebServer* server = nullptr;` (heap allocation)
  - **Razón:** Fragmentación de memoria en ESP32-C3 previene que AsyncTCP arranque
  - **Solución:** Crear solo cuando se necesita, eliminar al salir de CONFIG_MODE
  - **Trade-off:** Más complejo pero necesario para estabilidad

### 4. Detección de Duplicados en WiFi Scan
- **mica-gateway:** Usa librería `Vector.h`
- **pressure-gateway:** Usa búsqueda de string `indexOf()`
  - **Razón:** Reducir dependencias y uso de memoria
  - **Trade-off:** Ligeramente menos eficiente pero ahorra RAM

---

## ⚠️ Diferencias No Intencionadas (Revisar)

Estas diferencias **rompen la consistencia arquitectónica** y deben alinearse con mica-gateway:

### 1. ❌ Task Suspension at Creation (CRÍTICO)

**Estado actual (pressure-gateway):**
```cpp
xTaskCreate(..., &g_wifiConfigTaskHandle);
vTaskSuspend(g_wifiConfigTaskHandle);  // Suspendido al crear

xTaskCreate(..., &g_mqttConnectTaskHandle);
vTaskSuspend(g_mqttConnectTaskHandle);  // Suspendido al crear

xTaskCreate(..., &g_mqttTaskHandle);
vTaskSuspend(g_mqttTaskHandle);  // Suspendido al crear
```

**mica-gateway:**
- Todas las tareas **arrancan activas**
- El control de ejecución se hace **solo en `handleStateActions()`**

**Problema:**
- Patrón invertido respecto a la referencia
- Más complejo de debuggear (tareas nacen suspendidas)
- Dificulta identificar qué tarea debe estar activa en cada estado

**Solución recomendada:**
- Eliminar `vTaskSuspend()` tras creación
- Dejar que `handleStateActions()` controle el ciclo de vida completo

---

### 2. ❌ Button Task Suspended en CONNECTING State

**Estado actual (pressure-gateway):**
```cpp
case SYSTEM_STATE_CONNECTING:
    if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
```

**mica-gateway:**
```cpp
case SYSTEM_STATE_CONNECTING:
    if (g_buttonTaskHandle) vTaskResume(g_buttonTaskHandle);
```

**Problema:**
- En estado CONNECTING, el usuario **NO puede presionar el botón** para entrar en CONFIG_MODE
- mica-gateway mantiene el botón **activo** precisamente para detectar long press

**Solución recomendada:**
- Cambiar a `vTaskResume()` en CONNECTING state
- Mantener button task activa en todos los estados excepto ERROR/OTA

---

### 3. ❌ Sensor Task Suspended en CONNECTING State

**Estado actual (pressure-gateway):**
```cpp
case SYSTEM_STATE_CONNECTING:
    if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle);
```

**mica-gateway:**
```cpp
case SYSTEM_STATE_CONNECTING:
    if (g_pinReceiverTaskHandle) vTaskResume(g_pinReceiverTaskHandle);
```

**Problema:**
- Sensor de presión **debería estar activo** desde el inicio
- No tiene sentido suspender la adquisición mientras se conecta WiFi
- mica-gateway activa pin_receiver en CONNECTING

**Solución recomendada:**
- Cambiar a `vTaskResume()` en CONNECTING state
- La lectura de presión no interfiere con WiFi

---

### 4. ⚠️ MQTT Reconnection Logic

**Estado actual (pressure-gateway):**
```cpp
if (millis() - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = millis();
    if (!mqttClient.connect(...)) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
    }
}
```

**mica-gateway:**
```cpp
if (!mqttClient.connected()) {
    reconnect();  // Intento inmediato
}
```

**Problema:**
- Pressure-gateway añade rate limiting (5 segundos entre intentos)
- Más complejo, puede retrasar reconexión innecesariamente

**Impacto:** Medio - funciona pero diverge de la simplicidad de referencia

**Solución recomendada:**
- Simplificar a reconexión inmediata como mica-gateway
- Si rate limiting es necesario, debe estar en `reconnect()` no en el loop principal

---

### 5. ✅ OTA Validation Code (REVERTIDO)

**Estado previo:**
```cpp
#include <esp_ota_ops.h>

void setup() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}
```

**Estado actual:** ✅ **REVERTIDO** - Código eliminado

**Razón de la reversión:**
- OTA validation debe estar en `ota_manager.cpp`, no en `main.cpp`
- mica-gateway **NO tiene esta lógica** en setup()
- Con upload por USB, el rollback automático no aplica
- Mantiene consistencia arquitectónica

---

### 6. ✅ Debug Prints y Delays (REVERTIDO)

**Estado previo:**
```cpp
void setup() {
    Serial.begin(115200);
    delay(500);  // ❌ No está en mica-gateway
    
    Serial.println("ESP32-C3 Pressure Gateway - Production Mode");
    // ❌ Banner no está en mica-gateway
}
```

**Estado actual:** ✅ **REVERTIDO** - Simplificado como mica-gateway

---

## 📋 Stack Sizes - Alineación con mica-gateway

| Task                  | mica-gateway | pressure-gateway | ✅ Status |
|-----------------------|--------------|------------------|----------|
| wifiConnectTask       | 4096         | 4096             | ✅ Aligned |
| wifiConfigModeTask    | 4096         | 4096             | ✅ Aligned |
| mqttConnectTask       | 4096         | 4096             | ✅ Aligned |
| mqttPublishTask       | 10000        | 10000            | ✅ Aligned |
| stateManagementTask   | 4096         | 4096             | ✅ Aligned |
| ledTask               | 2048         | 2048             | ✅ Aligned |
| buttonTask            | 2048         | 2048             | ✅ Aligned |
| pressureReaderTask    | N/A          | 3072             | ⚠️ ESP32-C3 specific |
| pressureTelemetryTask | N/A          | 8192             | ⚠️ Matches telemetry |
| messageFormatterTask  | N/A          | 10240            | ⚠️ Needs ArduinoJson |

---

## 🎯 Acciones Recomendadas

### Alta Prioridad (Rompen arquitectura):
1. [ ] Eliminar `vTaskSuspend()` tras creación de tareas
2. [ ] Activar button task en CONNECTING state (no suspender)
3. [ ] Activar pressure reader en CONNECTING state (no suspender)

### Media Prioridad (Divergencia aceptable pero mejorable):
4. [ ] Simplificar lógica de reconexión MQTT (eliminar rate limiting del loop)
5. [ ] Documentar por qué AsyncWebServer usa heap allocation vs global

### Baja Prioridad (Optimizaciones válidas):
6. [x] ~~OTA validation en main.cpp~~ - **REVERTIDO** ✅
7. [x] ~~Debug banner y delays~~ - **REVERTIDO** ✅
8. [ ] Considerar re-añadir Vector.h si la memoria lo permite

---

## 📝 Notas de Diseño

### Por qué NO hacer pre-suspension de tareas:

**Filosofía de mica-gateway:**
- Tareas **siempre activas por defecto**
- State machine controla **cuándo pueden ejecutar lógica útil**
- Más fácil de debuggear (ves todas las tareas en estado Running)
- Menos overhead de suspender/reanudar constantemente

**Problema con pre-suspension:**
- Tareas nacen "muertas" → más difícil identificar problemas
- Requiere llamar explícitamente `vTaskResume()` antes de usar
- Puede causar deadlocks si olvidas reanudar una tarea crítica
- **No es el patrón usado en mica-gateway**

### Por qué button task debe estar activa en CONNECTING:

El propósito del button manager es **detectar long press para entrar en CONFIG_MODE**.

Si el sistema está atascado en CONNECTING (WiFi no disponible), el usuario necesita:
1. Mantener botón presionado 5 segundos
2. Entrar en CONFIG_MODE
3. Configurar nuevo SSID/password

**Si suspendemos button task en CONNECTING, el usuario queda bloqueado.**

---

## 🔄 Historial de Cambios

### 2025-11-25:
- ✅ Revertido: OTA validation code en main.cpp
- ✅ Revertido: Debug banner y delay(500) en setup()
- ⚠️ Pendiente: Task suspension at creation
- ⚠️ Pendiente: Button/sensor activation en CONNECTING state
- ⚠️ Pendiente: MQTT reconnection simplification

---

## 🎓 Lecciones Aprendidas

1. **Memoria limitada en ESP32-C3 requiere optimizaciones**, pero deben documentarse
2. **AsyncWebServer como pointer es válido** por limitación de hardware, no preferencia
3. **Task lifecycle debe seguir patrón de mica-gateway**: active por defecto, controladas por states
4. **OTA logic pertenece a ota_manager.cpp**, no a main.cpp
5. **Simplicidad > complejidad** - no añadir rate limiting si mica-gateway no lo necesita

---

**Última actualización:** 25 Noviembre 2025  
**Revisar periódicamente** cuando se sincronice con cambios de mica-gateway
